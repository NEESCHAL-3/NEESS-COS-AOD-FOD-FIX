#pragma once
#include <cerrno>
#include <linux/input.h>
#include <unistd.h>
namespace rodin {
constexpr int fodPollTimeout(bool active, bool hasStatusNode) {
    return hasStatusNode ? (active ? 8 : 250) : -1;
}
constexpr bool sampleFodStatus(bool hasStatusNode, bool hardwareEdge) {
    return hasStatusNode && !hardwareEdge;
}
// fd is opened O_NONBLOCK by findFodInput. Bound each drain so high-rate
// touchscreen events cannot starve the sysfs fallback.
template <typename Callback>
void drainFodInput(int fd, Callback callback) {
    input_event events[32];
    for (int batch = 0; batch < 4; ++batch) {
        ssize_t bytes = read(fd, events, sizeof(events));
        if (bytes < 0 && errno == EINTR) { --batch; continue; }
        if (bytes <= 0) break;
        for (size_t i = 0; i < static_cast<size_t>(bytes) / sizeof(input_event); ++i)
            callback(events[i]);
    }
}
}
