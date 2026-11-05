#include "../fod_input_policy.h"
#include "../fod_completion_policy.h"
#include <cassert>
#include <chrono>
#include <fcntl.h>
#include <vector>
int main() {
    // Simulate a synchronous success callback before the HAL call returns.
    bool armed=false, completed=true;
    int status=rodin::dispatchPreparedFod([&] { armed=true; completed=false; },
        [&] { assert(armed && !completed); completed=true; armed=false; return 0; },
        [] { assert(false); });
    assert(status==0 && completed && !armed);
    // Transport failure must also release the prepared capture state.
    status=rodin::dispatchPreparedFod([&] { armed=true; completed=false; },
        [] { return -1; }, [&] { armed=false; completed=true; });
    assert(status==-1 && completed && !armed);

    // After success, ON/OFF/DOZE and both idle-guard variants stay disarmed,
    // even when a stale operation/listener update is still in flight.
    for (bool on : {false, true}) for (bool active : {false, true})
        for (bool quirk : {false, true})
            assert(rodin::disarmCompletedFod(true, on, active, quirk));
    // A fresh authentication/enrollment or a new screen-off lock remains usable.
    for (bool quirk : {false, true}) {
        assert(!rodin::disarmCompletedFod(false, true, true, quirk));
        assert(!rodin::disarmCompletedFod(false, false, false, quirk));
    }
    assert(rodin::disarmCompletedFod(false, true, false, true));
    assert(!rodin::disarmCompletedFod(false, true, false, false));
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
