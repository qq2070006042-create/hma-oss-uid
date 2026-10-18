#pragma once

#include <cstdint>
#include <vector>

namespace hma_scope {

constexpr uint32_t kScopeMagic = 0x55414D48; // "HMAU"

struct Request {
    uint32_t magic;
    uint32_t uid;
    uint32_t pid;
    uint32_t tid;
    uint32_t length;
    char package[256];
};

// Reads the UID set that HMA hides from `package` in the given Android user.
bool load_hidden_uids(const char* package, uint32_t userId, std::vector<uint32_t>& out);

} // namespace hma_scope
