// ======================================================================
// \title  SeqStart.cpp
// \brief  Rule implementations for the seqStartIn port rule group
// ======================================================================

#include "STest/Random/Random.hpp"
#include "Svc/SeqDispatcher/test/ut/SeqDispatcherTester.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// SeqStart.Expected
// ----------------------------------------------------------------------

bool SeqDispatcherTester::SeqStart__Expected__precondition() const {
    return this->shadow.shadow_hasRunningSequencer();
}

void SeqDispatcherTester::SeqStart__Expected__action() {
    this->clearHistory();

    // Find a running sequencer (either block or no-block)
    FwIndexType idx = -1;
    if (this->shadow.shadow_hasBlockingSequencer()) {
        idx = this->shadow.shadow_getRandomSequencerInState(SeqDispatcher_CmdSequencerState::RUNNING_SEQUENCE_BLOCK);
    }
    if (idx == -1) {
        idx = this->shadow.shadow_getRandomSequencerInState(SeqDispatcher_CmdSequencerState::RUNNING_SEQUENCE_NO_BLOCK);
    }
    FW_ASSERT(idx >= 0);

    // Use the same filename as what the component expects
    Fw::String fileName(this->shadow.shadow_entryTable[idx].sequenceRunning);
    Svc::SeqArgs args{0, 0};

    this->invoke_to_seqStartIn(idx, fileName, args);
    this->component.doDispatch();

    // No state change needed - component already knows about this sequence
    ASSERT_EVENTS_SIZE(0);
}

// ----------------------------------------------------------------------
// SeqStart.Unexpected
// ----------------------------------------------------------------------

bool SeqDispatcherTester::SeqStart__Unexpected__precondition() const {
    return this->shadow.shadow_hasAvailableSequencer();
}

void SeqDispatcherTester::SeqStart__Unexpected__action() {
    this->clearHistory();

    FwIndexType idx = this->shadow.shadow_getRandomSequencerInState(SeqDispatcher_CmdSequencerState::AVAILABLE);
    FW_ASSERT(idx >= 0);

    Fw::String fileName("externally_started.bin");
    Svc::SeqArgs args{0, 0};

    this->invoke_to_seqStartIn(idx, fileName, args);
    this->component.doDispatch();

    // Update shadow: component marks it as running (NO_BLOCK since it was externally commanded)
    this->shadow.shadow_entryTable[idx].state = SeqDispatcher_CmdSequencerState::RUNNING_SEQUENCE_NO_BLOCK;
    this->shadow.shadow_entryTable[idx].sequenceRunning = fileName;
    this->shadow.shadow_sequencersAvailable--;

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_UnexpectedSequenceStarted_SIZE(1);
    ASSERT_EVENTS_UnexpectedSequenceStarted(0, static_cast<U16>(idx), fileName.toChar());

    ASSERT_TLM_sequencersAvailable_SIZE(1);
    ASSERT_TLM_sequencersAvailable(0, this->shadow.shadow_sequencersAvailable);
}

// ----------------------------------------------------------------------
// SeqStart.Conflicting
// ----------------------------------------------------------------------

bool SeqDispatcherTester::SeqStart__Conflicting__precondition() const {
    return this->shadow.shadow_hasRunningSequencer();
}

void SeqDispatcherTester::SeqStart__Conflicting__action() {
    this->clearHistory();

    // Find a running sequencer
    FwIndexType idx = -1;
    if (this->shadow.shadow_hasBlockingSequencer()) {
        idx = this->shadow.shadow_getRandomSequencerInState(SeqDispatcher_CmdSequencerState::RUNNING_SEQUENCE_BLOCK);
    }
    if (idx == -1) {
        idx = this->shadow.shadow_getRandomSequencerInState(SeqDispatcher_CmdSequencerState::RUNNING_SEQUENCE_NO_BLOCK);
    }
    FW_ASSERT(idx >= 0);

    Fw::String oldFileName(this->shadow.shadow_entryTable[idx].sequenceRunning);
    // Generate a filename guaranteed to differ from the current one
    char buf[64];
    U32 rnd = STest::Random::lowerUpper(0, 0xFFFF);
    (void)snprintf(buf, sizeof(buf), "conflict_%u.bin", rnd);
    Fw::String newFileName(buf);
    // Ensure it actually differs (extremely unlikely collision, but be safe)
    while (newFileName == oldFileName) {
        rnd = STest::Random::lowerUpper(0, 0xFFFF);
        (void)snprintf(buf, sizeof(buf), "conflict_%u.bin", rnd);
        newFileName = buf;
    }
    Svc::SeqArgs args{0, 0};

    this->invoke_to_seqStartIn(idx, newFileName, args);
    this->component.doDispatch();

    // Update shadow: component updates the sequence name
    this->shadow.shadow_entryTable[idx].sequenceRunning = newFileName;

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_ConflictingSequenceStarted_SIZE(1);
    ASSERT_EVENTS_ConflictingSequenceStarted(0, static_cast<U16>(idx), newFileName.toChar(), oldFileName.toChar());
}

}  // namespace Svc
