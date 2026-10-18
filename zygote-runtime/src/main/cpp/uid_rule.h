#pragma once
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>

#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

#include "uid_policy.h"

// Adapted from UID- (tools/uid-seccomp-module/filter.h, Apache-2.0).
// The rule makes getpriority(PRIO_USER, <other app uid>) fail with ESRCH inside
// the selected application process, which hides the existence of other apps
// from the uid probing technique.
namespace uid_filter {

inline std::vector<sock_filter> make_rule(uint32_t self, const std::vector<uint32_t>& uids,
                                          bool other_apps, bool check_arch = true) {
    std::vector<sock_filter> code;
    auto load = [&](uint32_t offset) {
        code.push_back(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offset));
    };
    auto equal = [&](uint32_t value, uint8_t yes, uint8_t no) {
        code.push_back(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, value, yes, no));
    };
    auto ret = [&](uint32_t value) {
        code.push_back(BPF_STMT(BPF_RET | BPF_K, value));
    };
    if (check_arch) {
        load(offsetof(seccomp_data, arch));
        equal(AUDIT_ARCH_AARCH64, 1, 0); ret(SECCOMP_RET_ALLOW);
    }
    load(offsetof(seccomp_data, nr));
    equal(__NR_getpriority, 1, 0); ret(SECCOMP_RET_ALLOW);
    // Kernel syscall parameters are int/id_t: compare their low 32 bits.
    load(offsetof(seccomp_data, args[0]));
    equal(PRIO_USER, 1, 0); ret(SECCOMP_RET_ALLOW);
    load(offsetof(seccomp_data, args[1]));
    if (other_apps) {
        equal(self, 0, 1); ret(SECCOMP_RET_ALLOW);
        const uint32_t first = self / 100000 * 100000 + 10000;
        code.push_back(BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, first, 1, 0));
        ret(SECCOMP_RET_ALLOW);
        code.push_back(BPF_JUMP(BPF_JMP | BPF_JGT | BPF_K, first + 9999, 0, 1));
        ret(SECCOMP_RET_ALLOW);
        ret(SECCOMP_RET_ERRNO | ESRCH);
    } else {
        for (auto uid : uids) { equal(uid, 0, 1); ret(SECCOMP_RET_ERRNO | ESRCH); }
    }
    ret(SECCOMP_RET_ALLOW);
    return code;
}

// Redirect only the platform policy's ALLOW leaves. Denials, architecture
// checks and all existing relative jumps retain their original behavior.
inline bool merge_rule(const sock_fprog& platform, uint32_t self,
                       const std::vector<sock_filter>& rule,
                       std::vector<sock_filter>& output) {
    if (!platform.filter || !platform.len || self % 100000 < 10000 || self % 100000 > 19999) {
        return false;
    }
    if (platform.len > BPF_MAXINSNS - rule.size()) return false;
    bool has_allow = false;
    for (size_t i = 0; i < platform.len; ++i) {
        const auto& op = platform.filter[i];
        if (BPF_CLASS(op.code) != BPF_RET) continue;
        if (op.code != (BPF_RET | BPF_K)) return false;
        if ((op.k & SECCOMP_RET_ACTION_FULL) == SECCOMP_RET_ALLOW && op.k != SECCOMP_RET_ALLOW) {
            return false;
        }
        has_allow |= op.k == SECCOMP_RET_ALLOW;
    }
    if (!has_allow) return false;
    if (detail::merge_with_budget(platform, rule, output)) return true;
    output.assign(platform.filter, platform.filter + platform.len);
    for (size_t i = 0; i < platform.len; ++i) {
        if (output[i].code == (BPF_RET | BPF_K) && output[i].k == SECCOMP_RET_ALLOW) {
            output[i] = BPF_STMT(BPF_JMP | BPF_JA, uint32_t(platform.len - i - 1));
        }
    }
    output.insert(output.end(), rule.begin(), rule.end());
    return true;
}

} // namespace uid_filter
