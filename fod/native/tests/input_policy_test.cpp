#include "../fod_input_policy.h"
#include <cassert>
#include <chrono>
#include <fcntl.h>
#include <vector>
int main() {
    int fd[2]; assert(pipe2(fd, O_NONBLOCK | O_CLOEXEC) == 0);
    input_event down{}; down.type=EV_KEY; down.code=0x152; down.value=1;
    assert(write(fd[1], &down, sizeof(down)) == sizeof(down));
    int count=0; auto start=std::chrono::steady_clock::now();
    rodin::drainFodInput(fd[0], [&](const input_event& ev) { assert(ev.value==1); ++count; });
    assert(count==1);
    assert(std::chrono::steady_clock::now()-start < std::chrono::milliseconds(100));
    rodin::drainFodInput(fd[0], [&](const input_event&) { assert(false); });
    std::vector<input_event> burst(200, down);
    assert(write(fd[1], burst.data(), burst.size()*sizeof(down)) == static_cast<ssize_t>(burst.size()*sizeof(down)));
    count=0; rodin::drainFodInput(fd[0], [&](const input_event&) { ++count; });
    assert(count==128);
    rodin::drainFodInput(fd[0], [&](const input_event&) { ++count; }); assert(count==200);
    assert(!rodin::sampleFodStatus(true,true));
    assert(rodin::sampleFodStatus(true,false));
    assert(rodin::fodPollTimeout(true,true) < rodin::fodPollTimeout(false,true));
    assert(rodin::fodPollTimeout(true,false)==-1);
    close(fd[1]);rodin::drainFodInput(fd[0], [&](const input_event&) { assert(false); });close(fd[0]);
}
