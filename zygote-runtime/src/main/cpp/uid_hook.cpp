#include "uid_hook.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "logger.hpp"
#include "uid_notification.h"

namespace uid_filter {

UidHook* UidHook::current_ = nullptr;

namespace {

bool valid_app_uid(uint32_t uid) {
    return uid % 100000 >= 10000 && uid % 100000 <= 19999;
}

} // namespace

void UidHook::prepare(zygisk::Api* api, uint32_t uid, const char* package) {
    if (prepared_) return;
    prepared_ = true;
    api_ = api;
    uid_ = uid;
    if (package != nullptr) snprintf(package_, sizeof(package_), "%s", package);

#if !defined(__aarch64__)
    return;
#endif
    if (api_ == nullptr || !valid_app_uid(uid_)) return;

    // Probe the companion first: it answers 'R' only when this app has a
    // non-empty per-target scope. Non-targets must not get a prctl hook.
    const int probe = api_->connectCompanion();
    if (probe < 0) return;
    const bool target = notification::prepare(probe, uid_);
    close(probe);
    if (!target) return;

    Dl_info info{};
    void* symbol = dlsym(RTLD_DEFAULT, "_Z22set_app_seccomp_filterv");
    if (symbol == nullptr) { LOGI("UID filter: seccomp symbol not found"); return; }
    if (dladdr(symbol, &info) == 0 || info.dli_fname == nullptr) return;
    LOGI("UID filter: runtime=%s", info.dli_fname);
    if (stat(info.dli_fname, &runtime_) != 0) return;

    current_ = this;
    api_->pltHookRegister(runtime_.st_dev, runtime_.st_ino, "prctl",
                          reinterpret_cast<void*>(intercept),
                          reinterpret_cast<void**>(&original_));
    const bool committed = api_->pltHookCommit();
    hook_active_ = original_ != nullptr;
    if (!committed && !hook_active_) current_ = nullptr;
    LOGI("UID filter: prctl=%d commit=%d", hook_active_ ? 1 : 0, committed ? 1 : 0);
}

int UidHook::intercept(int option, unsigned long second, unsigned long third,
                       unsigned long fourth, unsigned long fifth) {
    UidHook* self = current_;
    if (self == nullptr || self->original_ == nullptr) {
        return static_cast<int>(syscall(__NR_prctl, option, second, third, fourth, fifth));
    }
    if (option == PR_SET_SECCOMP && second == SECCOMP_MODE_FILTER && third != 0
            && !self->merged_) {
        const int saved_errno = errno;
        const int companion = self->api_->connectCompanion();
        const bool armed = companion >= 0 && notification::prepare(companion, self->uid_);
        if (armed) {
            const auto rule = notification::rule(self->uid_);
            const auto* platform = reinterpret_cast<const sock_fprog*>(third);
            std::vector<sock_filter> code;
            if (merge_rule(*platform, self->uid_, rule, code)) {
                const int listener = notification::install(code);
                if (listener >= 0) {
                    if (notification::send_listener(companion, listener)) {
                        close(listener);
                        close(companion);
                        self->merged_ = true;
                        LOGI("UID filter: merged via prctl");
                        return 0;
                    }
                    LOGI("UID filter: listener handoff failed");
                }
            }
        }
        if (companion >= 0) close(companion);
        errno = saved_errno;
    }
    return self->original_(option, second, third, fourth, fifth);
}

void UidHook::finish() {
    if (hook_active_ && original_ != nullptr) {
        void* displaced = nullptr;
        api_->pltHookRegister(runtime_.st_dev, runtime_.st_ino, "prctl",
                              reinterpret_cast<void*>(original_), &displaced);
        if (!api_->pltHookCommit() || displaced != reinterpret_cast<void*>(intercept)) {
            keep_loaded_ = true;
        } else {
            hook_active_ = false;
            current_ = nullptr;
        }
    }

    if (!merged_ && prepared_ && !keep_loaded_ && getuid() == uid_) {
        const int nnp = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
        if (nnp >= 0 && (nnp || prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0)) {
            const int companion = api_->connectCompanion();
            if (companion >= 0 && notification::prepare(companion, uid_)) {
                const auto rule = notification::rule(uid_);
                const int listener = notification::install(rule);
                if (listener >= 0) {
                    if (notification::send_listener(companion, listener)) merged_ = true;
                    close(listener);
                }
                close(companion);
            }
        }
    }

    LOGI("UID filter: status merged=%d", merged_ ? 1 : 0);
}

} // namespace uid_filter
