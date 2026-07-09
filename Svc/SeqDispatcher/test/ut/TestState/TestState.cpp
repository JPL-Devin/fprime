// ======================================================================
// \title  TestState.cpp
// \brief  Shadow helper implementations for SeqDispatcher test state
// ======================================================================

#include "Svc/SeqDispatcher/test/ut/TestState/TestState.hpp"
#include "STest/Random/Random.hpp"

namespace Svc {

FwIndexType SeqDispatcherTestState::shadow_getNextAvailableIdx() const {
    for (FwIndexType i = 0; i < SeqDispatcherSequencerPorts; i++) {
        if (this->shadow_entryTable[i].state == SeqDispatcher_CmdSequencerState::AVAILABLE) {
            return i;
        }
    }
    return -1;
}

bool SeqDispatcherTestState::shadow_hasAvailableSequencer() const {
    return this->shadow_getNextAvailableIdx() != -1;
}

bool SeqDispatcherTestState::shadow_hasBlockingSequencer() const {
    for (FwIndexType i = 0; i < SeqDispatcherSequencerPorts; i++) {
        if (this->shadow_entryTable[i].state == SeqDispatcher_CmdSequencerState::RUNNING_SEQUENCE_BLOCK) {
            return true;
        }
    }
    return false;
}

bool SeqDispatcherTestState::shadow_hasNonBlockingSequencer() const {
    for (FwIndexType i = 0; i < SeqDispatcherSequencerPorts; i++) {
        if (this->shadow_entryTable[i].state == SeqDispatcher_CmdSequencerState::RUNNING_SEQUENCE_NO_BLOCK) {
            return true;
        }
    }
    return false;
}

bool SeqDispatcherTestState::shadow_hasRunningSequencer() const {
    for (FwIndexType i = 0; i < SeqDispatcherSequencerPorts; i++) {
        if (this->shadow_entryTable[i].state != SeqDispatcher_CmdSequencerState::AVAILABLE) {
            return true;
        }
    }
    return false;
}

FwIndexType SeqDispatcherTestState::shadow_getRandomSequencerInState(SeqDispatcher_CmdSequencerState state) const {
    FwIndexType candidates[SeqDispatcherSequencerPorts];
    FwIndexType count = 0;
    for (FwIndexType i = 0; i < SeqDispatcherSequencerPorts; i++) {
        if (this->shadow_entryTable[i].state == state) {
            candidates[count++] = i;
        }
    }
    if (count == 0) {
        return -1;
    }
    U32 idx = STest::Random::lowerUpper(0, static_cast<U32>(count) - 1);
    return candidates[idx];
}

}  // namespace Svc
