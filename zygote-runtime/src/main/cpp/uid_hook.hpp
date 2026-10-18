#pragma once

#include <cstdint>
#include <sys/stat.h>

#include "ext/zygisk.hpp"

namespace uid_filter {

// Merges UID's seccomp user-notification rule into the platform policy. Every
// getpriority(PRIO_USER, <other app uid>) is handed to the root companion,
// which returns ESRCH only for the apps HMA hides from this package and lets
// the kernel continue otherwise.
class UidHook {
public:
    UidHook() = default;
    UidHook(const UidHook&) = delete;
    UidHook& operator=(const UidHook&) = delete;

    void prepare(zygisk::Api* api, uint32_t uid, const char* package);
    void finish();
    bool must_keep_loaded() const { return keep_loaded_; }

private:
    using Prctl = int (*)(int, unsigned long, unsigned long, unsigned long, unsigned long);

    static int intercept(int option, unsigned long second, unsigned long third,
                         unsigned long fourth, unsigned long fifth);

    static UidHook* current_;

    void log_status(bool merged);

    zygisk::Api* api_ = nullptr;
    Prctl original_ = nullptr;
    struct stat runtime_{};
    uint32_t uid_ = 0;
    char package_[256] = {};
    bool prepared_ = false;
    bool hook_active_ = false;
    bool merged_ = false;
    bool keep_loaded_ = false;
};

} // namespace uid_filter
