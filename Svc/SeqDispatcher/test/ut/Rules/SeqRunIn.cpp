// ======================================================================
// \title  SeqRunIn.cpp
// \brief  Rule implementations for the seqRunIn port rule group
// ======================================================================

#include "Svc/SeqDispatcher/test/ut/SeqDispatcherTester.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// SeqRunIn.Ok
// ----------------------------------------------------------------------

bool SeqDispatcherTester::SeqRunIn__Ok__precondition() const {
    return this->shadow.shadow_hasAvailableSequencer();
}

void SeqDispatcherTester::SeqRunIn__Ok__action() {
    this->clearHistory();

    FwIndexType expectedIdx = this->shadow.shadow_getNextAvailableIdx();
    Fw::String fileName("port_dispatch.bin");
    Svc::SeqArgs args{0, 0};

    this->invoke_to_seqRunIn(0, fileName, args);
    this->component.doDispatch();

    // Update shadow: seqRunIn always dispatches as NO_BLOCK
    this->shadow.shadow_entryTable[expectedIdx].state = SeqDispatcher_CmdSequencerState::RUNNING_SEQUENCE_NO_BLOCK;
    this->shadow.shadow_entryTable[expectedIdx].sequenceRunning = fileName;
    this->shadow.shadow_sequencersAvailable--;
    this->shadow.shadow_dispatchedCount++;

    ASSERT_from_seqRunOut_SIZE(1);

    ASSERT_TLM_dispatchedCount_SIZE(1);
    ASSERT_TLM_dispatchedCount(0, this->shadow.shadow_dispatchedCount);
    ASSERT_TLM_sequencersAvailable_SIZE(1);
    ASSERT_TLM_sequencersAvailable(0, this->shadow.shadow_sequencersAvailable);

    // No cmd response for port-based dispatch
    ASSERT_CMD_RESPONSE_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

// ----------------------------------------------------------------------
// SeqRunIn.NoSequencersAvailable
// ----------------------------------------------------------------------

bool SeqDispatcherTester::SeqRunIn__NoSequencersAvailable__precondition() const {
    return !this->shadow.shadow_hasAvailableSequencer();
}

void SeqDispatcherTester::SeqRunIn__NoSequencersAvailable__action() {
    this->clearHistory();

    Fw::String fileName("should_fail_port.bin");
    Svc::SeqArgs args{0, 0};

    this->invoke_to_seqRunIn(0, fileName, args);
    this->component.doDispatch();

    ASSERT_from_seqRunOut_SIZE(0);

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_NoAvailableSequencers_SIZE(1);

    // No cmd response for port-based dispatch
    ASSERT_CMD_RESPONSE_SIZE(0);
}

}  // namespace Svc
