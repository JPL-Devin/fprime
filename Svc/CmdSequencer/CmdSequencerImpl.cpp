// ======================================================================
// \title  CmdSequencerImpl.cpp
// \author Bocchino/Canham
// \brief  cpp file for CmdDispatcherComponentBase component implementation class
//
// Copyright (C) 2009-2018 California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.

#include <Fw/Com/ComPacket.hpp>
#include <Fw/Types/Assert.hpp>
#include <Fw/Types/ExternalString.hpp>
#include <Fw/Types/SerialBuffer.hpp>
#include <Fw/Types/Serializable.hpp>
#include <Svc/CmdSequencer/CmdSequencerImpl.hpp>
#include <Utils/Hash/Hash.hpp>
#include <config/CommandDispatcherImplCfg.hpp>

namespace Svc {

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

CmdSequencerComponentImpl::CmdSequencerComponentImpl(const char* name)
    : CmdSequencerComponentBase(name),
      m_FPrimeSequence(*this),
      m_sequence(&this->m_FPrimeSequence),
      m_loadCmdCount(0),
      m_cancelCmdCount(0),
      m_errorCount(0),
      m_runMode(STOPPED),
      m_stepMode(AUTO),
      m_executedCount(0),
      m_totalExecutedCount(0),
      m_sequencesCompletedCount(0),
      m_timeout(0),
      m_blockState(Svc::BlockState::NO_BLOCK),
      m_opCode(0),
      m_cmdSeq(0),
      m_join_waiting(false),
      m_lastCmdExecuted(false),
      m_lastCmdStatus(Fw::CmdResponse::OK),
      m_errorMode(true),  // Default: error mode ON (abort on error)
      m_errorPendingAbort(false),
      m_recordIndex(0) {}

void CmdSequencerComponentImpl::setTimeout(const U32 timeout) {
    this->m_timeout = timeout;
}

void CmdSequencerComponentImpl ::setSequenceFormat(Sequence& sequence) {
    this->m_sequence = &sequence;
}

void CmdSequencerComponentImpl ::allocateBuffer(const FwEnumStoreType identifier,
                                                Fw::MemAllocator& allocator,
                                                const FwSizeType bytes) {
    this->m_sequence->allocateBuffer(identifier, allocator, bytes);
}

void CmdSequencerComponentImpl ::loadSequence(const Fw::ConstStringBase& fileName) {
    FW_ASSERT(this->m_runMode == STOPPED, this->m_runMode);
    if (not this->loadFile(fileName)) {
        this->m_sequence->clear();
    }
}

void CmdSequencerComponentImpl ::deallocateBuffer(Fw::MemAllocator& allocator) {
    this->m_sequence->deallocateBuffer(allocator);
}

CmdSequencerComponentImpl::~CmdSequencerComponentImpl() {}

// ----------------------------------------------------------------------
// Handler implementations
// ----------------------------------------------------------------------

void CmdSequencerComponentImpl::CS_RUN_cmdHandler(FwOpcodeType opCode,
                                                  U32 cmdSeq,
                                                  const Fw::CmdStringArg& fileName,
                                                  const Svc::BlockState& block) {
    if (not this->requireRunMode(STOPPED, CmdSequencer_InvalidModeCause::RUN_NOT_STOPPED)) {
        if (m_join_waiting) {
            // Inform user previous seq file is not complete
            this->log_WARNING_HI_CS_JoinWaitingNotComplete();
        }
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    if ((Svc::BlockState::BLOCK == block.e) && (MANUAL == this->m_stepMode)) {
        // In MANUAL mode nothing executes until CS_STEP, so a BLOCK response could never be sent
        this->log_WARNING_HI_CS_InvalidMode(CmdSequencer_InvalidModeCause::RUN_BLOCK_IN_MANUAL);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->m_blockState = block.e;
    this->m_cmdSeq = cmdSeq;
    this->m_opCode = opCode;

    // load commands
    if (not this->loadFile(fileName)) {
        // Clear the recorded command state so a later port-driven run cannot
        // emit a duplicate response for this already-answered command
        this->m_blockState = Svc::BlockState::NO_BLOCK;
        this->m_opCode = 0;
        this->m_cmdSeq = 0;
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->m_executedCount = 0;

    // The response owed to this command is decided by the block state the command asked
    // for, not by m_blockState after stepping: a sequence that ends inside
    // performCmd_Step (an EXIT directive, an immediate end of sequence, or an abort)
    // clears m_blockState on its way out, and answering on the cleared value would send a
    // second response to a caller that sequenceComplete or performCmd_Cancel already
    // answered.
    const Svc::BlockState::t requestedBlock = block.e;
    StepResult stepResult = STEP_COMMAND_ISSUED;

    // Check the step mode. If it is auto, start the sequence
    if (AUTO == this->m_stepMode) {
        this->m_runMode = RUNNING;
        this->tlmWrite_CS_CurrentSequence(this->m_sequence->getStringFileName());
        if (this->isConnected_seqStartOut_OutputPort(0)) {
            // Create empty SeqArgs as placeholder
            // Use parameterized constructor to ensure m_size is initialized to 0
            Svc::SeqArgs emptyArgs{0, 0};
            this->seqStartOut_out(0, this->m_sequence->getStringFileName(), emptyArgs);
        }
        stepResult = this->performCmd_Step();
    }

    if (Svc::BlockState::NO_BLOCK == requestedBlock) {
        this->cmdResponse_out(
            opCode, cmdSeq,
            (STEP_SEQUENCE_ABORTED == stepResult) ? Fw::CmdResponse::EXECUTION_ERROR : Fw::CmdResponse::OK);
    }
}

void CmdSequencerComponentImpl::CS_VALIDATE_cmdHandler(FwOpcodeType opCode,
                                                       U32 cmdSeq,
                                                       const Fw::CmdStringArg& fileName) {
    FW_ASSERT(this->m_sequence != nullptr);
    if (!this->requireRunMode(STOPPED, CmdSequencer_InvalidModeCause::VALIDATE_NOT_STOPPED)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // load commands
    if (not this->loadFile(fileName)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // clear the buffer
    this->m_sequence->clear();

    this->log_ACTIVITY_HI_CS_SequenceValid(this->m_sequence->getLogFileName());

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

//! Handler for input port seqRunIn
void CmdSequencerComponentImpl::doSequenceRun(const Fw::StringBase& filename) {
    if (MANUAL == this->m_stepMode) {
        // In MANUAL mode nothing executes until CS_STEP, so a port-driven run would wedge
        this->log_WARNING_HI_CS_InvalidMode(CmdSequencer_InvalidModeCause::PORT_RUN_IN_MANUAL);
        this->seqDone_out(0, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    if (!this->requireRunMode(STOPPED, CmdSequencer_InvalidModeCause::PORT_RUN_NOT_STOPPED)) {
        this->seqDone_out(0, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // If file name is non-empty, load a file.
    // Empty file name means don't load.
    if (filename != "") {
        Fw::CmdStringArg cmdStr(filename);
        const bool status = this->loadFile(cmdStr);
        if (!status) {
            this->seqDone_out(0, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }
    } else if (not this->m_sequence->hasMoreRecords()) {
        // No sequence loaded
        this->log_WARNING_LO_CS_NoSequenceActive();
        this->error();
        this->seqDone_out(0, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->m_executedCount = 0;

    // Check the step mode. If it is auto, start the sequence
    if (AUTO == this->m_stepMode) {
        this->m_runMode = RUNNING;
        this->tlmWrite_CS_CurrentSequence(this->m_sequence->getStringFileName());
        if (this->isConnected_seqStartOut_OutputPort(0)) {
            // Create empty SeqArgs as placeholder
            // Use parameterized constructor to ensure m_size is initialized to 0
            Svc::SeqArgs emptyArgs{0, 0};
            this->seqStartOut_out(0, this->m_sequence->getStringFileName(), emptyArgs);
        }
        // Logged before the first step so that a sequence which ends or aborts inside
        // that step reports its start before its end
        this->log_ACTIVITY_HI_CS_PortSequenceStarted(this->m_sequence->getLogFileName());
        // A port-driven run has no command to answer: the step reports its own outcome
        // through events and, when the sequence ends or aborts, through seqDone
        (void)this->performCmd_Step();
    }
}

void CmdSequencerComponentImpl::seqRunIn_handler(FwIndexType portNum,
                                                 const Fw::StringBase& filename,
                                                 const Svc::SeqArgs& args) {
    (void)args;  // Suppress unused parameter warning
    this->doSequenceRun(filename);
}

void CmdSequencerComponentImpl::seqDispatchIn_handler(FwIndexType portNum, Fw::StringBase& file_name) {
    this->doSequenceRun(file_name);
}

void CmdSequencerComponentImpl ::seqCancelIn_handler(const FwIndexType portNum) {
    if (RUNNING == this->m_runMode) {
        this->performCmd_Cancel();
        this->log_ACTIVITY_HI_CS_SequenceCanceled(this->m_sequence->getLogFileName());
        ++this->m_cancelCmdCount;
        this->tlmWrite_CS_CancelCommands(this->m_cancelCmdCount);
    } else {
        this->log_WARNING_LO_CS_NoSequenceActive();
    }
}

void CmdSequencerComponentImpl::CS_CANCEL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (RUNNING == this->m_runMode) {
        this->performCmd_Cancel();
        this->log_ACTIVITY_HI_CS_SequenceCanceled(this->m_sequence->getLogFileName());
        ++this->m_cancelCmdCount;
        this->tlmWrite_CS_CancelCommands(this->m_cancelCmdCount);
    } else {
        this->log_WARNING_LO_CS_NoSequenceActive();
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void CmdSequencerComponentImpl::CS_JOIN_WAIT_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) {
    // If there is no running sequence do not wait
    if (m_runMode != RUNNING) {
        this->log_WARNING_LO_CS_NoSequenceActive();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    } else if ((Svc::BlockState::BLOCK == this->m_blockState) || this->m_join_waiting) {
        // A command response is already owed to a BLOCK-mode CS_RUN caller or a
        // previous CS_JOIN_WAIT caller. Reject rather than overwrite that state,
        // which would leave the original caller without a completion response.
        this->log_WARNING_HI_CS_JoinWaitingNotComplete();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    } else {
        m_join_waiting = true;
        Fw::LogStringArg& logFileName = this->m_sequence->getLogFileName();
        this->log_ACTIVITY_HI_CS_JoinWaiting(logFileName, m_cmdSeq, CmdDispatcherCfg::getEventOpcode(m_opCode));
        m_cmdSeq = cmdSeq;
        m_opCode = opCode;
    }
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

bool CmdSequencerComponentImpl ::loadFile(const Fw::ConstStringBase& fileName) {
    const bool status = this->m_sequence->loadFile(fileName);
    if (status) {
        this->clearSequenceState();
        Fw::LogStringArg& logFileName = this->m_sequence->getLogFileName();
        this->log_ACTIVITY_LO_CS_SequenceLoaded(logFileName);
        ++this->m_loadCmdCount;
        this->tlmWrite_CS_LoadCommands(this->m_loadCmdCount);
    } else {
        // A partial load may have populated m_buffer before an intermediate
        // validation step (e.g. CRC, time, record structure) failed. Without
        // an explicit reset, FPrimeSequence::hasMoreRecords() still returns
        // true because getDeserializeSizeLeft() > 0, so a subsequent CS_START
        // bypasses the "no sequence active" guard and reaches
        // FPrimeSequence::nextRecord, which asserts on the failed deserialize
        // and aborts the FSW. Clearing the sequence on every load failure
        // closes that window.
        this->m_sequence->clear();
    }
    return status;
}

void CmdSequencerComponentImpl::error() {
    ++this->m_errorCount;
    this->tlmWrite_CS_Errors(m_errorCount);
}

void CmdSequencerComponentImpl::performCmd_Cancel() {
    FW_ASSERT(this->m_sequence != nullptr);
    this->m_sequence->reset();
    this->m_runMode = STOPPED;
    this->m_cmdTimer.clear();
    this->m_cmdTimeoutTimer.clear();
    this->m_executedCount = 0;
    this->clearSequenceState();

    // write sequence done port with error, if connected
    if (this->isConnected_seqDone_OutputPort(0)) {
        this->seqDone_out(0, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
    }

    if (Svc::BlockState::BLOCK == this->m_blockState || m_join_waiting) {
        // Do not wait if sequence was canceled or a cmd failed
        this->m_join_waiting = false;
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }

    this->m_blockState = Svc::BlockState::NO_BLOCK;
}

void CmdSequencerComponentImpl::abortOnCommandError() {
    // Clear the flag before canceling so that the cancel path cannot be re-entered by it,
    // and so that a subsequent sequence does not inherit a pending abort
    this->m_errorPendingAbort = false;
    this->performCmd_Cancel();
}

void CmdSequencerComponentImpl ::cmdResponseIn_handler(FwIndexType portNum,
                                                       FwOpcodeType opcode,
                                                       U32 cmdSeq,
                                                       const Fw::CmdResponse& response) {
    if (this->m_runMode == STOPPED) {
        // Sequencer is not running
        this->log_WARNING_HI_CS_UnexpectedCompletion(CmdDispatcherCfg::getEventOpcode(opcode));
    } else {
        // clear command timeout
        this->m_cmdTimeoutTimer.clear();

        // Store the last command status for JCF/JCS directives that come next
        this->m_lastCmdExecuted = true;
        this->m_lastCmdStatus = response;

        if (response != Fw::CmdResponse::OK) {
            // Command failed - log error and continue execution to give JCF a chance to handle
            this->commandError(this->m_executedCount, opcode, response.e);

            // A failed command still occupies a command number. Advancing the count keeps
            // the numbers carried by later CS_CommandComplete and CS_CommandError events
            // sequential; only the CS_CommandsExecuted channel is reserved for commands
            // that succeeded.
            ++this->m_executedCount;

            if (this->m_errorMode) {
                // Error mode is ON - mark that we need to abort unless JCF handles it
                this->m_errorPendingAbort = true;
            }

            // Continue to next record (which might be a JCF directive)
            if (this->m_runMode == RUNNING && this->m_stepMode == AUTO) {
                // Auto mode - continue to next record
                if (not this->m_sequence->hasMoreRecords()) {
                    // No data left. A sequence may legally end on a command record, so the
                    // failure has to be reported here: with error mode ON no directive can
                    // follow to handle it.
                    this->m_runMode = STOPPED;
                    if (this->m_errorPendingAbort) {
                        this->abortOnCommandError();
                    } else {
                        this->sequenceComplete();
                    }
                } else {
                    // The step reports its own outcome through events and, when the
                    // sequence ends or aborts, through seqDone and the blocked command
                    // response; there is no caller here to answer
                    (void)this->performCmd_Step();
                }
            } else {
                // Manual step mode - wait for next step command
                if (not this->m_sequence->hasMoreRecords()) {
                    this->m_runMode = STOPPED;
                    if (this->m_errorPendingAbort) {
                        this->abortOnCommandError();
                    } else {
                        this->sequenceComplete();
                    }
                }
            }
        } else {
            // Command succeeded - continue normally
            if (this->m_runMode == RUNNING && this->m_stepMode == AUTO) {
                // Auto mode
                this->commandComplete(opcode);
                if (not this->m_sequence->hasMoreRecords()) {
                    // No data left
                    this->m_runMode = STOPPED;
                    this->sequenceComplete();
                } else {
                    // See the failed-command branch above for why the result is unused
                    (void)this->performCmd_Step();
                }
            } else {
                // Manual step mode
                this->commandComplete(opcode);
                if (not this->m_sequence->hasMoreRecords()) {
                    this->m_runMode = STOPPED;
                    this->sequenceComplete();
                }
            }
        }
    }
}

void CmdSequencerComponentImpl ::schedIn_handler(FwIndexType portNum, U32 order) {
    Fw::Time currTime = this->getTime();
    // check to see if a command time is pending
    if (this->m_cmdTimer.isExpiredAt(currTime)) {
        this->comCmdOut_out(0, m_record.m_command, 0);
        this->m_cmdTimer.clear();
        // start command timeout timer
        this->setCmdTimeout(currTime);
    } else if (this->m_cmdTimeoutTimer.isExpiredAt(this->getTime())) {  // check for command timeout
        this->log_WARNING_HI_CS_SequenceTimeout(m_sequence->getLogFileName(), this->m_executedCount);
        // If there is a command timeout, cancel the sequence
        this->performCmd_Cancel();
    }
}

void CmdSequencerComponentImpl ::CS_START_cmdHandler(FwOpcodeType opcode, U32 cmdSeq) {
    if (not this->m_sequence->hasMoreRecords()) {
        // No sequence loaded
        this->log_WARNING_LO_CS_NoSequenceActive();
        this->cmdResponse_out(opcode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    if (!this->requireRunMode(STOPPED, CmdSequencer_InvalidModeCause::START_NOT_STOPPED)) {
        this->cmdResponse_out(opcode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->m_blockState = Svc::BlockState::NO_BLOCK;
    this->m_runMode = RUNNING;
    this->tlmWrite_CS_CurrentSequence(this->m_sequence->getStringFileName());
    this->log_ACTIVITY_HI_CS_CmdStarted(this->m_sequence->getLogFileName());
    const StepResult stepResult = this->performCmd_Step();
    if (this->isConnected_seqStartOut_OutputPort(0)) {
        // Create empty SeqArgs as placeholder
        Svc::SeqArgs emptyArgs{0, 0};
        this->seqStartOut_out(0, this->m_sequence->getStringFileName(), emptyArgs);
    }
    this->cmdResponse_out(
        opcode, cmdSeq, (STEP_SEQUENCE_ABORTED == stepResult) ? Fw::CmdResponse::EXECUTION_ERROR : Fw::CmdResponse::OK);
}

void CmdSequencerComponentImpl ::CS_STEP_cmdHandler(FwOpcodeType opcode, U32 cmdSeq) {
    FW_ASSERT(this->m_sequence != nullptr);
    if (this->requireRunMode(RUNNING, CmdSequencer_InvalidModeCause::STEP_NOT_RUNNING)) {
        if (MANUAL != this->m_stepMode) {
            // CS_STEP is valid only in MANUAL step mode
            this->log_WARNING_HI_CS_InvalidMode(CmdSequencer_InvalidModeCause::STEP_NOT_MANUAL);
            this->cmdResponse_out(opcode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }
        if (not this->m_sequence->hasMoreRecords()) {
            // A sequence with no end-of-sequence record leaves nothing to step; stepping anyway
            // asserts in the sequence reader
            this->log_WARNING_LO_CS_NoSequenceActive();
            this->cmdResponse_out(opcode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }
        // An orderly end of sequence stops the sequencer and is a successful step; an
        // abort also stops it but must not be acknowledged as OK
        const StepResult stepResult = this->performCmd_Step();
        if ((STEP_COMMAND_ISSUED == stepResult) || (STEP_DIRECTIVE_DONE == stepResult)) {
            this->log_ACTIVITY_HI_CS_CmdStepped(this->m_sequence->getLogFileName(), this->m_executedCount);
        }
        this->cmdResponse_out(
            opcode, cmdSeq,
            (STEP_SEQUENCE_ABORTED == stepResult) ? Fw::CmdResponse::EXECUTION_ERROR : Fw::CmdResponse::OK);
    } else {
        this->cmdResponse_out(opcode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

void CmdSequencerComponentImpl ::CS_AUTO_cmdHandler(FwOpcodeType opcode, U32 cmdSeq) {
    if (this->requireRunMode(STOPPED, CmdSequencer_InvalidModeCause::AUTO_NOT_STOPPED)) {
        this->m_stepMode = AUTO;
        this->log_ACTIVITY_HI_CS_ModeSwitched(CmdSequencer_SeqMode::AUTO);
        this->cmdResponse_out(opcode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        this->cmdResponse_out(opcode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

void CmdSequencerComponentImpl ::CS_MANUAL_cmdHandler(FwOpcodeType opcode, U32 cmdSeq) {
    if (this->requireRunMode(STOPPED, CmdSequencer_InvalidModeCause::MANUAL_NOT_STOPPED)) {
        this->m_stepMode = MANUAL;
        this->log_ACTIVITY_HI_CS_ModeSwitched(CmdSequencer_SeqMode::STEP);
        this->cmdResponse_out(opcode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        this->cmdResponse_out(opcode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

bool CmdSequencerComponentImpl::requireRunMode(RunMode mode, CmdSequencer_InvalidModeCause::T cause) {
    if (this->m_runMode == mode) {
        return true;
    } else {
        this->log_WARNING_HI_CS_InvalidMode(cause);
        return false;
    }
}

void CmdSequencerComponentImpl ::commandError(const U32 number, const FwOpcodeType opCode, const U32 error) {
    this->log_WARNING_HI_CS_CommandError(this->m_sequence->getLogFileName(), number,
                                         CmdDispatcherCfg::getEventOpcode(opCode), error);
    this->error();
}

CmdSequencerComponentImpl::StepResult CmdSequencerComponentImpl::performCmd_Step() {
    FW_ASSERT(this->m_sequence != nullptr);
    const Sequence::Header& header = this->m_sequence->getHeader();

    // Bound on the number of records one step may consume. Directives do not issue
    // commands, so a step reads records until it reaches a command, the end of the
    // sequence, or a directive that ends or aborts the sequence.
    //
    // The sequence holds m_numRecords records, established and validated at load time.
    // Reading more than that many records within a single step therefore proves that some
    // record was read twice, which can only happen if a jump was taken; and a jump with no
    // command in between is a directive cycle. So the bound is both a termination proof
    // (CPP-27) and the detection mechanism for a cyclic sequence.
    //
    // Both sequence formats populate m_numRecords before any record is read, but the bound
    // is floored at 1 rather than asserted so that a format which does not cannot wedge
    // the sequencer.
    const U32 recordLimit = (header.m_numRecords > 0) ? header.m_numRecords : 1;

    U32 recordIndex = this->m_recordIndex;
    for (U32 recordsRead = 0; recordsRead < recordLimit; ++recordsRead) {
        this->m_sequence->nextRecord(this->m_record);
        recordIndex = this->m_recordIndex;
        ++this->m_recordIndex;
        // set clock time base and context from value set when sequence was loaded
        this->m_record.m_timeTag.setTimeBase(header.m_timeBase);
        this->m_record.m_timeTag.setTimeContext(header.m_timeContext);

        // A command failed while error mode was ON. Only a JCF directive immediately after
        // that command can handle the failure, so any other kind of record ends the
        // sequence here.
        if (this->m_errorPendingAbort && (this->m_record.m_descriptor != Sequence::Record::SEQUENCE_DIRECTIVE)) {
            this->abortOnCommandError();
            return STEP_SEQUENCE_ABORTED;
        }

        Fw::Time currentTime = this->getTime();
        switch (this->m_record.m_descriptor) {
            case Sequence::Record::END_OF_SEQUENCE:
                this->m_runMode = STOPPED;
                this->sequenceComplete();
                return STEP_SEQUENCE_ENDED;
            case Sequence::Record::RELATIVE:
                this->performCmd_Step_RELATIVE(currentTime);
                return STEP_COMMAND_ISSUED;
            case Sequence::Record::ABSOLUTE:
                this->performCmd_Step_ABSOLUTE(currentTime);
                return STEP_COMMAND_ISSUED;
            case Sequence::Record::SEQUENCE_DIRECTIVE: {
                const DirectiveResult directiveResult = this->executeDirective(this->m_record, recordIndex);
                switch (directiveResult) {
                    case DIRECTIVE_CONTINUE:
                    case DIRECTIVE_JUMPED:
                        // Keep reading records in this step until one issues a command or
                        // ends the sequence
                        break;
                    case DIRECTIVE_SEQUENCE_ENDED:
                        // The directive reported completion itself
                        return STEP_SEQUENCE_ENDED;
                    case DIRECTIVE_ABORT_PENDING_ERROR:
                        // A command failed with error mode ON and this directive is not the
                        // JCF that would have handled it. The cancel is issued here, once,
                        // by the owner of the step.
                        this->abortOnCommandError();
                        return STEP_SEQUENCE_ABORTED;
                    case DIRECTIVE_FAILED:
                        // executeDirective has already reported the specific failure
                        this->performCmd_Cancel();
                        return STEP_SEQUENCE_ABORTED;
                    default:
                        FW_ASSERT(false, static_cast<FwAssertArgType>(directiveResult));
                        return STEP_SEQUENCE_ABORTED;
                }
                break;
            }
            default:
                FW_ASSERT(false, this->m_record.m_descriptor);
                return STEP_SEQUENCE_ABORTED;
        }

        // Only a directive that wants execution to continue reaches here.
        if (not this->m_sequence->hasMoreRecords()) {
            // A sequence may end without an END_OF_SEQUENCE record. Checked before the
            // manual-mode return so that a trailing directive completes the sequence in
            // either step mode instead of leaving a manual sequence RUNNING with nothing
            // left to step.
            this->m_runMode = STOPPED;
            this->sequenceComplete();
            return STEP_SEQUENCE_ENDED;
        }
        if (MANUAL == this->m_stepMode) {
            // One CS_STEP consumes one record, so stop after the directive rather than
            // running ahead to the next command
            return STEP_DIRECTIVE_DONE;
        }
    }

    // Directives redirected execution over more records than the sequence contains without
    // reaching a command: the sequence contains a cycle of jumps. The last record read is
    // the jump that closed the cycle.
    (void)this->directiveError(recordIndex, CmdSequencer_DirectiveError::DIRECTIVE_CYCLE);
    this->performCmd_Cancel();
    return STEP_SEQUENCE_ABORTED;
}

void CmdSequencerComponentImpl::sequenceComplete() {
    this->sequenceComplete(Fw::CmdResponse::OK);
}

void CmdSequencerComponentImpl::sequenceComplete(const Fw::CmdResponse& status) {
    FW_ASSERT(this->m_sequence != nullptr);
    ++this->m_sequencesCompletedCount;
    // reset buffer
    this->m_sequence->clear();
    this->log_ACTIVITY_HI_CS_SequenceComplete(this->m_sequence->getLogFileName());
    this->tlmWrite_CS_SequencesCompleted(this->m_sequencesCompletedCount);
    this->m_executedCount = 0;
    this->clearSequenceState();

    // write sequence done port, if connected
    if (this->isConnected_seqDone_OutputPort(0)) {
        this->seqDone_out(0, 0, 0, status);
    }

    if (Svc::BlockState::BLOCK == this->m_blockState || m_join_waiting) {
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, status);
    }

    m_join_waiting = false;
    this->m_blockState = Svc::BlockState::NO_BLOCK;
    this->tlmWrite_CS_CurrentSequence(NO_SEQ);
}

void CmdSequencerComponentImpl::clearSequenceState() {
    this->m_lastCmdExecuted = false;
    this->m_lastCmdStatus = Fw::CmdResponse::OK;
    // Error mode defaults to ON for every sequence
    this->m_errorMode = true;
    this->m_errorPendingAbort = false;
    this->m_recordIndex = 0;
}

void CmdSequencerComponentImpl::commandComplete(const FwOpcodeType opcode) {
    this->log_ACTIVITY_LO_CS_CommandComplete(this->m_sequence->getLogFileName(), this->m_executedCount,
                                             CmdDispatcherCfg::getEventOpcode(opcode));
    ++this->m_executedCount;
    ++this->m_totalExecutedCount;
    this->tlmWrite_CS_CommandsExecuted(this->m_totalExecutedCount);
}

void CmdSequencerComponentImpl ::performCmd_Step_RELATIVE(Fw::Time& currentTime) {
    this->m_record.m_timeTag.add(currentTime.getSeconds(), currentTime.getUSeconds());
    this->performCmd_Step_ABSOLUTE(currentTime);
}

void CmdSequencerComponentImpl ::performCmd_Step_ABSOLUTE(Fw::Time& currentTime) {
    if (currentTime >= this->m_record.m_timeTag) {
        this->comCmdOut_out(0, m_record.m_command, 0);
        this->setCmdTimeout(currentTime);
    } else {
        this->m_cmdTimer.set(this->m_record.m_timeTag);
    }
}

void CmdSequencerComponentImpl ::pingIn_handler(FwIndexType portNum, /*!< The port number*/
                                                U32 key              /*!< Value to return to pinger*/
) {
    // send ping response
    this->pingOut_out(0, key);
}

void CmdSequencerComponentImpl ::setCmdTimeout(const Fw::Time& currentTime) {
    // start timeout timer if enabled and not in step mode
    if ((this->m_timeout > 0) and (AUTO == this->m_stepMode)) {
        Fw::Time expTime = currentTime;
        expTime.add(this->m_timeout, 0);
        this->m_cmdTimeoutTimer.set(expTime);
    }
}

Fw::SerializeStatus CmdSequencerComponentImpl ::deserializeLabel(Fw::LinearBufferBase& buffer, Fw::StringBase& label) {
    U8 labelLength = 0;
    Fw::SerializeStatus status = buffer.deserializeTo(labelLength);
    if (status != Fw::FW_SERIALIZE_OK) {
        return status;
    }
    if (labelLength > Sequence::Record::MAX_LABEL_SIZE) {
        return Fw::FW_DESERIALIZE_SIZE_MISMATCH;
    }

    // A directive label is written on the wire as a U8 length followed by that many raw
    // characters. That is not the Fw::StringBase serialization format, which prefixes an
    // FwSizeStoreType, so StringBase::deserializeFrom cannot be used here without changing
    // the sequence file format.
    char labelBuffer[Sequence::Record::LABEL_BUFFER_SIZE] = {};
    FwSizeType readSize = labelLength;
    // reinterpret_cast justification: the raw entry point of deserializeTo takes U8* and
    // there is no CHAR* overload. The cast target is used only as the destination of a byte
    // copy into labelBuffer, which is then read as characters; no object is accessed
    // through an incompatible type, and U8 and char have the same size and alignment.
    status = buffer.deserializeTo(reinterpret_cast<U8*>(labelBuffer), readSize, Fw::Serialization::OMIT_LENGTH);
    if (status != Fw::FW_SERIALIZE_OK) {
        return status;
    }
    // deserializeTo only ever lowers readSize, and labelLength was bounded above, so the
    // terminator is written within labelBuffer
    FW_ASSERT(readSize <= Sequence::Record::MAX_LABEL_SIZE, static_cast<FwAssertArgType>(readSize));
    labelBuffer[readSize] = '\0';
    label = labelBuffer;
    return Fw::FW_SERIALIZE_OK;
}

CmdSequencerComponentImpl::DirectiveResult CmdSequencerComponentImpl ::directiveError(
    const U32 recordIndex,
    CmdSequencer_DirectiveError::T error) {
    this->log_WARNING_HI_CS_DirectiveError(this->m_sequence->getLogFileName(), recordIndex, error);
    this->error();
    return DIRECTIVE_FAILED;
}

CmdSequencerComponentImpl::DirectiveResult CmdSequencerComponentImpl ::executeDirective(Sequence::Record& record,
                                                                                        const U32 recordIndex) {
    FW_ASSERT(this->m_sequence != nullptr);

    // The directive payload is read in place from the record's own buffer. Deserializing
    // through the record rather than through a separate view over its address is what lets
    // this function avoid casting away the const of a buffer it does not modify.
    Fw::LinearBufferBase& dirBuf = record.m_command;
    dirBuf.resetDeser();

    U8 directiveId = 0;
    Fw::SerializeStatus status = dirBuf.deserializeTo(directiveId);
    if (status != Fw::FW_SERIALIZE_OK) {
        return this->directiveError(recordIndex, CmdSequencer_DirectiveError::DIRECTIVE_ID_UNREADABLE);
    }
    if (not Sequence::Record::DirectiveId::isValid(directiveId)) {
        return this->directiveError(recordIndex, CmdSequencer_DirectiveError::UNKNOWN_DIRECTIVE);
    }
    const Sequence::Record::DirectiveId directive(directiveId);

    // A command failed while error mode was ON. Only a JCF in the record immediately after
    // that command handles the failure; see "Directive State Management" in docs/sdd.md.
    // The abort itself belongs to performCmd_Step, which owns the cancel.
    if (this->m_errorPendingAbort && (directive != Sequence::Record::DirectiveId::JCF)) {
        return DIRECTIVE_ABORT_PENDING_ERROR;
    }

    switch (directive.e) {
        case Sequence::Record::DirectiveId::LABEL:
            // A label marks a jump target. There is nothing to execute.
            return DIRECTIVE_CONTINUE;

        case Sequence::Record::DirectiveId::JCF:
        case Sequence::Record::DirectiveId::JCS: {
            // Both directives test the status of the command in the immediately preceding
            // record; they differ only in which outcome takes the jump.
            if (not this->m_lastCmdExecuted) {
                return this->directiveError(recordIndex, CmdSequencer_DirectiveError::NO_PRIOR_COMMAND);
            }

            char labelBuffer[Sequence::Record::LABEL_BUFFER_SIZE] = {};
            Fw::ExternalString label(labelBuffer, sizeof(labelBuffer));
            status = CmdSequencerComponentImpl::deserializeLabel(dirBuf, label);
            if (status != Fw::FW_SERIALIZE_OK) {
                return this->directiveError(recordIndex, CmdSequencer_DirectiveError::LABEL_UNREADABLE);
            }

            const bool priorCmdSucceeded = (this->m_lastCmdStatus == Fw::CmdResponse::OK);
            const bool takeJump =
                (directive == Sequence::Record::DirectiveId::JCS) ? priorCmdSucceeded : (not priorCmdSucceeded);
            if (not takeJump) {
                // The condition did not hold, so execution falls through to the next record
                return DIRECTIVE_CONTINUE;
            }

            if (not this->jumpToLabel(label)) {
                return this->directiveError(recordIndex, CmdSequencer_DirectiveError::LABEL_NOT_FOUND);
            }

            this->log_ACTIVITY_HI_CS_DirectiveJump(this->m_sequence->getLogFileName(), directive, label);

            // The jump consumed the command status that selected it. Clearing it here keeps
            // a JCF or JCS at the jump target from re-testing a command several records
            // back, and is what makes a retry loop terminate rather than jump forever.
            // Consecutive JCF/JCS after one command still all see that command, because the
            // status is cleared only when a jump is actually taken.
            this->m_lastCmdExecuted = false;
            this->m_lastCmdStatus = Fw::CmdResponse::OK;
            // A JCF that jumped has handled the failure
            this->m_errorPendingAbort = false;
            return DIRECTIVE_JUMPED;
        }

        case Sequence::Record::DirectiveId::EXIT: {
            U8 exitStatus = 0;
            status = dirBuf.deserializeTo(exitStatus);
            if (status != Fw::FW_SERIALIZE_OK) {
                return this->directiveError(recordIndex, CmdSequencer_DirectiveError::EXIT_STATUS_UNREADABLE);
            }
            if (exitStatus > 1) {
                return this->directiveError(recordIndex, CmdSequencer_DirectiveError::EXIT_STATUS_INVALID);
            }

            this->m_runMode = STOPPED;
            const Fw::CmdResponse exitResponse =
                (exitStatus == 0) ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR;
            this->sequenceComplete(exitResponse);
            return DIRECTIVE_SEQUENCE_ENDED;
        }

        case Sequence::Record::DirectiveId::ERROR_MODE: {
            U8 mode = 0;
            status = dirBuf.deserializeTo(mode);
            if (status != Fw::FW_SERIALIZE_OK) {
                return this->directiveError(recordIndex, CmdSequencer_DirectiveError::ERROR_MODE_UNREADABLE);
            }
            if (mode > 1) {
                return this->directiveError(recordIndex, CmdSequencer_DirectiveError::ERROR_MODE_INVALID);
            }

            // 1 = ON (abort on command error), 0 = OFF (continue on command error)
            this->m_errorMode = (mode == 1);
            return DIRECTIVE_CONTINUE;
        }

        default:
            // Unreachable: DirectiveId::isValid above rejects every value that has no case
            FW_ASSERT(false, static_cast<FwAssertArgType>(directive.e));
            return this->directiveError(recordIndex, CmdSequencer_DirectiveError::UNKNOWN_DIRECTIVE);
    }
}

bool CmdSequencerComponentImpl ::jumpToLabel(const Fw::ConstStringBase& labelName) {
    FW_ASSERT(this->m_sequence != nullptr);

    // Search from the start of the sequence so that a label before the current position is
    // reachable. Jump direction is unrestricted; see docs/sdd.md.
    const U32 numRecords = this->m_sequence->getHeader().m_numRecords;
    this->m_sequence->reset();

    // Counted rather than driven by hasMoreRecords alone: the record count is fixed at load
    // time, which makes the bound on this search explicit.
    for (U32 recordNumber = 0; recordNumber < numRecords; ++recordNumber) {
        if (not this->m_sequence->hasMoreRecords()) {
            break;
        }
        Sequence::Record searchRecord;
        this->m_sequence->nextRecord(searchRecord);
        if (searchRecord.m_descriptor != Sequence::Record::SEQUENCE_DIRECTIVE) {
            continue;
        }

        Fw::LinearBufferBase& dirBuf = searchRecord.m_command;
        dirBuf.resetDeser();

        U8 directiveId = 0;
        Fw::SerializeStatus status = dirBuf.deserializeTo(directiveId);
        if (status != Fw::FW_SERIALIZE_OK) {
            // Malformed uplinked content is never discarded without a trace
            this->log_WARNING_HI_CS_LabelRecordInvalid(this->m_sequence->getLogFileName(), recordNumber,
                                                       CmdSequencer_DirectiveError::DIRECTIVE_ID_UNREADABLE);
            continue;
        }
        if (not Sequence::Record::DirectiveId::isValid(directiveId)) {
            this->log_WARNING_HI_CS_LabelRecordInvalid(this->m_sequence->getLogFileName(), recordNumber,
                                                       CmdSequencer_DirectiveError::UNKNOWN_DIRECTIVE);
            continue;
        }
        const Sequence::Record::DirectiveId directive(directiveId);
        if (directive != Sequence::Record::DirectiveId::LABEL) {
            // Only labels can be jump targets. Any other directive is executed when
            // execution reaches it, not during the search.
            continue;
        }

        char labelBuffer[Sequence::Record::LABEL_BUFFER_SIZE] = {};
        Fw::ExternalString label(labelBuffer, sizeof(labelBuffer));
        status = CmdSequencerComponentImpl::deserializeLabel(dirBuf, label);
        if (status != Fw::FW_SERIALIZE_OK) {
            this->log_WARNING_HI_CS_LabelRecordInvalid(this->m_sequence->getLogFileName(), recordNumber,
                                                       CmdSequencer_DirectiveError::LABEL_UNREADABLE);
            continue;
        }

        if (labelName == label) {
            // The sequence deserializer is now positioned just past this label record, so
            // the next record read is the first record after the label
            this->m_recordIndex = recordNumber + 1;
            return true;
        }
    }

    // Label not found
    return false;
}

}  // namespace Svc
