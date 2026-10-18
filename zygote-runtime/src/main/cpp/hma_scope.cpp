#include "hma_scope.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

#include "logger.hpp"
#include "uid_scope_file.h"

namespace hma_scope {
namespace {

constexpr size_t kMaxPackage = 255;
constexpr off_t kMaxScopeFile = 4 * 1024 * 1024;

bool send_all(int fd, const void* data, size_t size) {
    auto* bytes = static_cast<const char*>(data);
    while (size > 0) {
        const ssize_t sent = send(fd, bytes, size, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (sent == 0) return false;
        bytes += sent;
        size -= static_cast<size_t>(sent);
    }
    return true;
}

bool recv_all(int fd, void* data, size_t size) {
    auto* bytes = static_cast<char*>(data);
    while (size > 0) {
        const ssize_t received = recv(fd, bytes, size, 0);
        if (received < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (received == 0) return false;
        bytes += received;
        size -= static_cast<size_t>(received);
    }
    return true;
}

bool find_scope_path(char* output, size_t size) {
    DIR* misc = opendir("/data/misc");
    if (misc == nullptr) return false;

    char best[PATH_MAX] = {};
    bool have_scope = false;
    time_t newest_time = 0;

    while (const dirent* entry = readdir(misc)) {
        if (strncmp(entry->d_name, "hide_my_applist", 15) != 0) continue;

        char path[sizeof("/data/misc/") + NAME_MAX + sizeof("/process_scope")];
        const int written = snprintf(path, sizeof(path), "/data/misc/%s/process_scope",
                                     entry->d_name);
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(path)) continue;

        struct stat info{};
        if (stat(path, &info) != 0) continue;
        if (!have_scope || info.st_mtime >= newest_time) {
            snprintf(best, sizeof(best), "%s", path);
            newest_time = info.st_mtime;
            have_scope = true;
        }
    }
    closedir(misc);
    if (!have_scope) return false;
    snprintf(output, size, "%s", best);
    return true;
}

char* read_scope_file(const char* path, size_t& length) {
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return nullptr;

    struct stat info{};
    if (fstat(fd, &info) != 0 || info.st_size <= 0 || info.st_size > kMaxScopeFile) {
        close(fd);
        return nullptr;
    }

    auto* data = static_cast<char*>(malloc(static_cast<size_t>(info.st_size) + 1));
    if (data == nullptr) {
        close(fd);
        return nullptr;
    }

    size_t used = 0;
    const size_t total = static_cast<size_t>(info.st_size);
    while (used < total) {
        const ssize_t count = read(fd, data + used, total - used);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        used += static_cast<size_t>(count);
    }
    close(fd);
    data[used] = '\0';
    length = used;
    return data;
}

} // namespace

bool load_hidden_uids(const char* package, uint32_t userId, std::vector<uint32_t>& out) {
    char path[PATH_MAX];
    if (!find_scope_path(path, sizeof(path))) return false;

    size_t length = 0;
    char* data = read_scope_file(path, length);
    if (data == nullptr) return false;

    bool found = false;
    char* save_line = nullptr;
    for (char* line = strtok_r(data, "\n", &save_line); line != nullptr;
         line = strtok_r(nullptr, "\n", &save_line)) {
        char* save_token = nullptr;
        char* token = strtok_r(line, " \t\r", &save_token);
        if (token == nullptr || strcmp(token, package) != 0) continue;

        char* user_token = strtok_r(nullptr, " \t\r", &save_token);
        if (user_token == nullptr) continue;

        char* user_end = nullptr;
        const unsigned long user = strtoul(user_token, &user_end, 10);
        if (user_end == nullptr || *user_end != '\0' || user != userId) continue;

        while (char* uid_token = strtok_r(nullptr, " \t\r", &save_token)) {
            char* uid_end = nullptr;
            const unsigned long value = strtoul(uid_token, &uid_end, 10);
            if (uid_end == nullptr || *uid_end != '\0' || value == 0) continue;
            out.push_back(static_cast<uint32_t>(value));
        }
        found = true;
        break;
    }

    free(data);
    return found;
}

int prepare(zygisk::Api* api, const char* package, uint32_t uid) {
    if (api == nullptr || package == nullptr) return -1;
    const size_t length = strlen(package);
    if (length == 0 || length > kMaxPackage) return -1;

    const int fd = api->connectCompanion();
    if (fd < 0) return -1;

    timeval timeout{2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    Request request{};
    request.magic = kScopeMagic;
    request.uid = uid;
    request.pid = static_cast<uint32_t>(getpid());
    request.tid = static_cast<uint32_t>(syscall(__NR_gettid));
    request.length = static_cast<uint32_t>(length);
    memcpy(request.package, package, length);

    char ready = 0;
    if (!send_all(fd, &request, sizeof(request)) || !recv_all(fd, &ready, 1)
            || ready != 'R') {
        close(fd);
        return -1;
    }
    return fd;
}

bool is_target(zygisk::Api* api, const char* package, uint32_t uid) {
    const int fd = prepare(api, package, uid);
    if (fd < 0) return false;
    close(fd);
    return true;
}

bool send_listener(int socket, int listener) {
    char byte = 'F';
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(listener))]{};
    iovec payload{&byte, 1};
    msghdr message{};
    message.msg_iov = &payload;
    message.msg_iovlen = 1;
    message.msg_control = control;
    message.msg_controllen = sizeof(control);

    auto* header = CMSG_FIRSTHDR(&message);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(sizeof(listener));
    memcpy(CMSG_DATA(header), &listener, sizeof(listener));

    ssize_t result;
    do {
        result = sendmsg(socket, &message, MSG_NOSIGNAL);
    } while (result < 0 && errno == EINTR);
    return result == 1;
}

} // namespace hma_scope
