// ======================================================================
// \title  LogStatusCmd.cpp
// \brief  Rule implementations for the LOG_STATUS command rule group
// ======================================================================

#include "STest/Random/Random.hpp"
#include "Svc/SeqDispatcher/test/ut/SeqDispatcherTester.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// LogStatusCmd.Ok
// ----------------------------------------------------------------------

bool SeqDispatcherTester::LogStatusCmd__Ok__precondition() const {
    return true;
}

void SeqDispatcherTester::LogStatusCmd__Ok__action() {
    this->clearHistory();

    U32 cmdSeq = STest::Random::lowerUpper(0, 0xFFFF);

    this->sendCmd_LOG_STATUS(0, cmdSeq);
    this->component.doDispatch();

    ASSERT_EVENTS_SIZE(SeqDispatcherSequencerPorts);

    for (FwIndexType idx = 0; idx < SeqDispatcherSequencerPorts; idx++) {
        ASSERT_EVENTS_LogSequencerStatus(idx, static_cast<U16>(idx), this->shadow.shadow_entryTable[idx].state,
                                         this->shadow.shadow_entryTable[idx].sequenceRunning.toChar());
    }

    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, SeqDispatcher::OPCODE_LOG_STATUS, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Svc
