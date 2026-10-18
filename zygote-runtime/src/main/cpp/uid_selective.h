#pragma once

#include <cerrno>
#include <cstdint>
#include <vector>

#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <unistd.h>

#include "uid_rule.h"

namespace uid_filter::notification {

// Rule used inside the target process: every getpriority(PRIO_USER, <other app
// uid>) is turned into a user notification. The companion decides whether the
// queried UID is hidden and answers with ESRCH, or asks the kernel for the real
// value so non-hidden queries look exactly like an unfiltered syscall.
inline std::vector<sock_filter> rule(uint32_t self) {
    auto code = make_rule(self, {}, true);
    for (auto& instruction : code) {
        if (instruction.code == (BPF_RET | BPF_K)
                && instruction.k == (SECCOMP_RET_ERRNO | ESRCH)) {
            instruction.k = SECCOMP_RET_USER_NOTIF;
        }
    }
    return code;
}

inline int install(const std::vector<sock_filter>& code) {
    sock_fprog program{};
    program.len = static_cast<unsigned short>(code.size());
    program.filter = const_cast<sock_filter*>(code.data());
    return static_cast<int>(syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER,
                                    SECCOMP_FILTER_FLAG_NEW_LISTENER, &program));
}

} // namespace uid_filter::notification
