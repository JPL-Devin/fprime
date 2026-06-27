// ======================================================================
// \title  SeqDone.cpp
// \brief  Rule implementations for the seqDoneIn port rule group
// ======================================================================

#include "STest/Random/Random.hpp"
#include "Svc/SeqDispatcher/test/ut/SeqDispatcherTester.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// SeqDone.KnownBlock
// ----------------------------------------------------------------------

bool SeqDispatcherTester::SeqDone__KnownBlock__precondition() const {
    return this->shadow.shadow_hasBlockingSequencer();
}

void SeqDispatcherTester::SeqDone__KnownBlock__action() {
    this->clearHistory();

    FwIndexType idx =
        this->shadow.shadow_getRandomSequencerInState(SeqDispatcher_CmdSequencerState::RUNNING_SEQUENCE_BLOCK);
    FW_ASSERT(idx >= 0);

    FwOpcodeType savedOpCode = this->shadow.shadow_entryTable[idx].opCode;
    U32 savedCmdSeq = this->shadow.shadow_entryTable[idx].cmdSeq;
    Fw::CmdResponse response = STest::Random::lowerUpper(0, 1) ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR;

    this->invoke_to_seqDoneIn(idx, savedOpCode, savedCmdSeq, response);
    this->component.doDispatch();

    // Update shadow
    this->shadow.shadow_entryTable[idx].state = SeqDispatcher_CmdSequencerState::AVAILABLE;
    this->shadow.shadow_entryTable[idx].sequenceRunning = "<no seq>";
    this->shadow.shadow_sequencersAvailable++;

    if (response == Fw::CmdResponse::EXECUTION_ERROR) {
        this->shadow.shadow_errorCount++;
        ASSERT_TLM_errorCount_SIZE(1);
        ASSERT_TLM_errorCount(0, this->shadow.shadow_errorCount);
    }

    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, savedOpCode, savedCmdSeq, response);

    ASSERT_TLM_sequencersAvailable_SIZE(1);
    ASSERT_TLM_sequencersAvailable(0, this->shadow.shadow_sequencersAvailable);

    ASSERT_EVENTS_SIZE(0);
}

// ----------------------------------------------------------------------
// SeqDone.KnownNoBlock
// ----------------------------------------------------------------------

bool SeqDispatcherTester::SeqDone__KnownNoBlock__precondition() const {
    return this->shadow.shadow_hasNonBlockingSequencer();
}

void SeqDispatcherTester::SeqDone__KnownNoBlock__action() {
    this->clearHistory();

    FwIndexType idx =
        this->shadow.shadow_getRandomSequencerInState(SeqDispatcher_CmdSequencerState::RUNNING_SEQUENCE_NO_BLOCK);
    FW_ASSERT(idx >= 0);

    this->invoke_to_seqDoneIn(idx, 0, 0, Fw::CmdResponse::OK);
    this->component.doDispatch();

    // Update shadow
    this->shadow.shadow_entryTable[idx].state = SeqDispatcher_CmdSequencerState::AVAILABLE;
    this->shadow.shadow_entryTable[idx].sequenceRunning = "<no seq>";
    this->shadow.shadow_sequencersAvailable++;

    // No cmd response for non-blocking
    ASSERT_CMD_RESPONSE_SIZE(0);

    ASSERT_TLM_sequencersAvailable_SIZE(1);
    ASSERT_TLM_sequencersAvailable(0, this->shadow.shadow_sequencersAvailable);

    ASSERT_EVENTS_SIZE(0);
}

// ----------------------------------------------------------------------
// SeqDone.Unknown
// ----------------------------------------------------------------------

bool SeqDispatcherTester::SeqDone__Unknown__precondition() const {
    return this->shadow.shadow_hasAvailableSequencer();
}

void SeqDispatcherTester::SeqDone__Unknown__action() {
    this->clearHistory();

    FwIndexType idx = this->shadow.shadow_getRandomSequencerInState(SeqDispatcher_CmdSequencerState::AVAILABLE);
    FW_ASSERT(idx >= 0);

    this->invoke_to_seqDoneIn(idx, 0, 0, Fw::CmdResponse::OK);
    this->component.doDispatch();

    // Shadow: sequencer was already available, but component increments its counter
    // and resets state. We need to track this carefully.
    // The component sets state to AVAILABLE and increments sequencersAvailable
    // even though it was already available, so our shadow must match.
    this->shadow.shadow_sequencersAvailable++;

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_UnknownSequenceFinished_SIZE(1);
    ASSERT_EVENTS_UnknownSequenceFinished(0, static_cast<U16>(idx));

    ASSERT_TLM_sequencersAvailable_SIZE(1);
    ASSERT_TLM_sequencersAvailable(0, this->shadow.shadow_sequencersAvailable);
}

}  // namespace Svc
