#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>

// Adapted from UID- (tools/uid-seccomp-module/policy.h, Apache-2.0).
// Keeps the instruction budget of the platform filter when the UID rule is
// merged into it, so the kernel's per-path seccomp limits do not change.
namespace uid_filter::detail {
// The AArch64 kernel converts seccomp cBPF to eBPF before accounting for
// MAX_INSNS_PER_PATH. Count both representations; equal cBPF lengths alone
// do not preserve the amount of policy an application can still install.
inline size_t converted_size(const std::vector<sock_filter>& code) {
    size_t count = 3; // cBPF A/X initialization and context register.
    for (const auto& op : code) {
        if (op.code == (BPF_RET | BPF_K)) { count += 2; continue; }
        if (BPF_CLASS(op.code) == BPF_JMP && BPF_OP(op.code) != BPF_JA) {
            count += 1 + (op.k > INT32_MAX);
            if (op.jf && (op.jt || BPF_OP(op.code) == BPF_JSET)) ++count;
            continue;
        }
        ++count;
    }
    return count;
}

// Accept only Android's architecture dispatch followed by independent,
// forward-only syscall-number policies. Argument-dependent or unfamiliar
// platform policies keep the existing merge path.
inline bool number_policy(const sock_fprog& p, size_t begin, size_t end) {
    if (begin >= end || p.filter[begin].code != (BPF_LD | BPF_W | BPF_ABS)
            || p.filter[begin].k != offsetof(seccomp_data, nr)) return false;
    for (size_t i = begin; i < end; ++i) {
        const auto& op = p.filter[i];
        if (op.code == (BPF_RET | BPF_K)) {
            if (op.k != SECCOMP_RET_ALLOW && op.k != SECCOMP_RET_TRAP) return false;
        } else if (op.code == (BPF_LD | BPF_W | BPF_ABS)) {
            if (op.k != offsetof(seccomp_data, nr) || i + 1 >= end) return false;
        } else if (op.code == (BPF_JMP | BPF_JA)) {
            if (uint64_t(i) + 1 + op.k >= end) return false;
        } else if (op.code == (BPF_JMP | BPF_JEQ | BPF_K)
                || op.code == (BPF_JMP | BPF_JGE | BPF_K)
                || op.code == (BPF_JMP | BPF_JGT | BPF_K)) {
            if (i + 1 + op.jt >= end || i + 1 + op.jf >= end) return false;
        } else return false;
    }
    return p.filter[end - 1].code == (BPF_RET | BPF_K);
}

inline uint32_t evaluate_number(const sock_fprog& p, size_t pc, uint32_t nr) {
    // number_policy has already proved every path ends in a fixed return.
    for (;;) {
        const auto& op = p.filter[pc];
        if (op.code == (BPF_RET | BPF_K)) return op.k;
        if (op.code == (BPF_LD | BPF_W | BPF_ABS)) { ++pc; continue; }
        if (op.code == (BPF_JMP | BPF_JA)) { pc += 1 + op.k; continue; }
        const bool yes = BPF_OP(op.code) == BPF_JEQ ? nr == op.k
            : BPF_OP(op.code) == BPF_JGE ? nr >= op.k : nr > op.k;
        pc += 1 + (yes ? op.jt : op.jf);
    }
}

inline bool merge_with_budget(const sock_fprog& p, const std::vector<sock_filter>& rule,
                              std::vector<sock_filter>& output) {
    if (p.len < 8) return false;
    const auto* f = p.filter;
    if (f[0].code != (BPF_LD | BPF_W | BPF_ABS) || f[0].k != offsetof(seccomp_data, arch)
            || f[1].code != (BPF_JMP | BPF_JEQ | BPF_K) || f[1].k != AUDIT_ARCH_AARCH64
            || f[1].jt != 2 || f[1].jf != 0
            || f[2].code != (BPF_JMP | BPF_JEQ | BPF_K) || f[2].k != AUDIT_ARCH_ARM || f[2].jf != 0
            || f[3].code != (BPF_RET | BPF_K) || f[3].k != SECCOMP_RET_TRAP) return false;
    const size_t arm = 3 + f[2].jt;
    if (arm <= 4 || arm >= p.len || !number_policy(p, 4, arm) || !number_policy(p, arm, p.len)) return false;

    // Keep the native syscall decision tree. Compact only the compatibility
    // tree into exact 32-syscall bitmaps, preserving every allowed/denied nr.
    std::vector<uint64_t> edges{0, uint64_t{1} << 32};
    for (size_t i = arm; i < p.len; ++i) {
        if (BPF_CLASS(f[i].code) == BPF_JMP && BPF_OP(f[i].code) != BPF_JA) {
            edges.push_back(f[i].k);
            edges.push_back(uint64_t(f[i].k) + 1);
        }
    }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    std::map<uint32_t, uint32_t> masks;
    for (size_t i = 1; i < edges.size(); ++i) {
        const uint64_t first = edges[i - 1], last = edges[i] - 1;
        if (evaluate_number(p, arm, uint32_t(first)) != SECCOMP_RET_ALLOW) continue;
        if ((last >> 5) - (first >> 5) > 128) return false;
        for (uint64_t bucket = first >> 5; bucket <= last >> 5; ++bucket) {
            const uint64_t base = bucket << 5;
            const uint64_t low = std::max(first, base) - base;
            const uint64_t high = std::min(last, base + 31) - base;
            masks[uint32_t(bucket)] |= uint32_t(((uint64_t{1} << (high - low + 1)) - 1) << low);
            if (masks.size() > 128) return false;
        }
    }
    std::vector<sock_filter> compact(f, f + arm);
    compact.push_back(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr)));
    compact.push_back(BPF_STMT(BPF_ALU | BPF_AND | BPF_K, 31));
    compact.push_back(BPF_STMT(BPF_MISC | BPF_TAX, 0));
    compact.push_back(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr)));
    compact.push_back(BPF_STMT(BPF_ALU | BPF_RSH | BPF_K, 5));
    std::vector<size_t> jumps;
    for (const auto& [bucket, mask] : masks) {
        compact.push_back(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, bucket, 0, 2));
        compact.push_back(BPF_STMT(BPF_LD | BPF_W | BPF_IMM, mask));
        jumps.push_back(compact.size());
        compact.push_back(BPF_STMT(BPF_JMP | BPF_JA, 0));
    }
    compact.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP));
    const size_t match = compact.size();
    for (size_t i : jumps) compact[i].k = uint32_t(match - i - 1);
    compact.push_back(BPF_STMT(BPF_ALU | BPF_RSH | BPF_X, 0));
    compact.push_back(BPF_JUMP(BPF_JMP | BPF_JSET | BPF_K, 1, 1, 0));
    compact.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP));
    compact.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));
    for (size_t i = 4; i < arm; ++i) {
        if (compact[i].code == (BPF_RET | BPF_K) && compact[i].k == SECCOMP_RET_ALLOW)
            compact[i] = BPF_STMT(BPF_JMP | BPF_JA, 0);
    }
    const size_t length = compact.size() + rule.size();
    const size_t cost = converted_size(compact) + converted_size(rule) - 3;
    const size_t original_cost = converted_size(std::vector<sock_filter>(f, f + p.len));
    if (length > p.len || cost > original_cost) return false;
    const size_t padding = p.len - length, missing = original_cost - cost;
    if (missing < padding || missing - padding > padding) return false;
    // All compatibility paths have already returned. These unreachable entries
    // reserve the original cBPF and converted instruction budgets; no delay is
    // added to a syscall. RET_K costs two converted instructions, LD_IMM one.
    for (size_t i = 0; i < padding; ++i)
        compact.push_back(i < missing - padding
            ? sock_filter BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)
            : sock_filter BPF_STMT(BPF_LD | BPF_W | BPF_IMM, 0));
    const size_t entry = compact.size();
    for (size_t i = 4; i < arm; ++i) {
        if (f[i].code == (BPF_RET | BPF_K) && f[i].k == SECCOMP_RET_ALLOW)
            compact[i].k = uint32_t(entry - i - 1);
    }
    compact.insert(compact.end(), rule.begin(), rule.end());
    if (compact.size() != p.len || converted_size(compact) != original_cost) return false;
    output.swap(compact);
    return true;
}
}
