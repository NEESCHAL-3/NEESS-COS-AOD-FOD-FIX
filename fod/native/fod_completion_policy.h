#pragma once

namespace rodin {
// Completion is universal. Existing pre-auth idle quirks remain scoped so
// screen-off/pending touch handling is not changed for otherwise healthy HALs.
inline bool disarmCompletedFod(bool completed, bool screenOn,
                               bool operationActive, bool idleGuardNeeded) {
    return completed || (idleGuardNeeded && screenOn && !operationActive);
}
// Completion can arrive before the real HAL transaction returns.
template<class Prepare, class Dispatch, class Abort>
auto dispatchPreparedFod(Prepare prepare, Dispatch dispatch, Abort abort) {
    prepare();
    auto status = dispatch();
    if (status != 0) abort();
    return status;
}

}
