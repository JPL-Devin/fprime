// ======================================================================
// \title  RunCmd.cpp
// \brief  Rule implementations for the RUN command rule group
// ======================================================================

#include "STest/Random/Random.hpp"
#include "Svc/SeqDispatcher/test/ut/SeqDispatcherTester.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// RunCmd.Ok
// ----------------------------------------------------------------------

bool SeqDispatcherTester::RunCmd__Ok__precondition() const {
    return this->shadow.shadow_hasAvailableSequencer();
}

void SeqDispatcherTester::RunCmd__Ok__action() {
    this->clearHistory();

    BlockState block = STest::Random::lowerUpper(0, 1) ? BlockState::BLOCK : BlockState::NO_BLOCK;
    U32 cmdSeq = STest::Random::lowerUpper(0, 0xFFFF);
    Fw::String fileName("seq_file.bin");

    FwIndexType expectedIdx = this->shadow.shadow_getNextAvailableIdx();

    this->sendCmd_RUN(0, cmdSeq, fileName, block);
    this->component.doDispatch();

    // Update shadow state
    if (block == BlockState::BLOCK) {
        this->shadow.shadow_entryTable[expectedIdx].state = SeqDispatcher_CmdSequencerState::RUNNING_SEQUENCE_BLOCK;
    } else {
        this->shadow.shadow_entryTable[expectedIdx].state = SeqDispatcher_CmdSequencerState::RUNNING_SEQUENCE_NO_BLOCK;
    }
    this->shadow.shadow_entryTable[expectedIdx].sequenceRunning = fileName;
    this->shadow.shadow_entryTable[expectedIdx].opCode = SeqDispatcher::OPCODE_RUN;
    this->shadow.shadow_entryTable[expectedIdx].cmdSeq = cmdSeq;
    this->shadow.shadow_sequencersAvailable--;
    this->shadow.shadow_dispatchedCount++;

    // Verify output port was invoked
    ASSERT_from_seqRunOut_SIZE(1);

    // Verify telemetry
    ASSERT_TLM_dispatchedCount_SIZE(1);
    ASSERT_TLM_dispatchedCount(0, this->shadow.shadow_dispatchedCount);
    ASSERT_TLM_sequencersAvailable_SIZE(1);
    ASSERT_TLM_sequencersAvailable(0, this->shadow.shadow_sequencersAvailable);

    if (block == BlockState::NO_BLOCK) {
        ASSERT_CMD_RESPONSE_SIZE(1);
        ASSERT_CMD_RESPONSE(0, SeqDispatcher::OPCODE_RUN, cmdSeq, Fw::CmdResponse::OK);
    } else {
        ASSERT_CMD_RESPONSE_SIZE(0);
    }

    ASSERT_EVENTS_SIZE(0);
}

// ----------------------------------------------------------------------
// RunCmd.NoSequencersAvailable
// ----------------------------------------------------------------------

bool SeqDispatcherTester::RunCmd__NoSequencersAvailable__precondition() const {
    return !this->shadow.shadow_hasAvailableSequencer();
}

void SeqDispatcherTester::RunCmd__NoSequencersAvailable__action() {
    this->clearHistory();

    BlockState block = STest::Random::lowerUpper(0, 1) ? BlockState::BLOCK : BlockState::NO_BLOCK;
    U32 cmdSeq = STest::Random::lowerUpper(0, 0xFFFF);
    Fw::String fileName("should_fail.bin");

    this->sendCmd_RUN(0, cmdSeq, fileName, block);
    this->component.doDispatch();

    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, SeqDispatcher::OPCODE_RUN, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_NoAvailableSequencers_SIZE(1);

    ASSERT_from_seqRunOut_SIZE(0);
}

}  // namespace Svc
