// ======================================================================
// \title  Directives.cpp
// \author Canham
// \brief  Tests for sequence directives
//
// \copyright
// Copyright (C) 2009-2018 California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
// ======================================================================

#include "Svc/CmdSequencer/test/ut/Directives.hpp"
#include "Svc/CmdSequencer/test/ut/CommandBuffers.hpp"

namespace Svc {

namespace Directives {

namespace {

typedef CmdSequencer_DirectiveId DirectiveId;
typedef CmdSequencer_DirectiveStatus DirectiveStatus;
typedef CmdSequencer_InvalidModeCause InvalidModeCause;

//! A label longer than Sequence::Record::MAX_LABEL_SIZE, which is 20 characters
const char* const OVERLONG_LABEL = "123456789012345678901";

}  // namespace

// ----------------------------------------------------------------------
// Constructors
// ----------------------------------------------------------------------

CmdSequencerTester ::CmdSequencerTester(const SequenceFiles::File::Format::t a_format)
    : Svc::CmdSequencerTester(a_format) {}

// ----------------------------------------------------------------------
// Tests: one directive at a time
// ----------------------------------------------------------------------

void CmdSequencerTester ::LabelIsSkipped() {
    SequenceFiles::DirectiveFile file("label_skipped");
    file.label("START").command(0, 1).endOfSequence();
    const char* const fileName = this->prepare(file);

    // The label is stepped over within the same step that issues the command, so the run
    // command reports success and logs nothing but the load
    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // The command completes and the end-of-sequence record ends the sequence
    this->respond(0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);
    ASSERT_EVENTS_CS_SequenceComplete(0, fileName);
    ASSERT_from_seqDone_SIZE(1);
    ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::OK));
}

void CmdSequencerTester ::ExitOk() {
    SequenceFiles::DirectiveFile file("exit_ok");
    file.command(0, 1).exitSeq(0);
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // The EXIT directive ends the sequence in place of an end-of-sequence record
    this->respond(0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);
    ASSERT_EVENTS_CS_SequenceComplete(0, fileName);
    ASSERT_from_seqDone_SIZE(1);
    ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::OK));
    ASSERT_EQ(CmdSequencerComponentImpl::STOPPED, this->component.m_runMode);
}

void CmdSequencerTester ::ExitError() {
    SequenceFiles::DirectiveFile file("exit_error");
    file.command(0, 1).exitSeq(1);
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // EXIT 1 still completes the sequence, but reports failure to the caller
    this->respond(0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);
    ASSERT_EVENTS_CS_SequenceComplete(0, fileName);
    ASSERT_from_seqDone_SIZE(1);
    ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::EXECUTION_ERROR));
}

void CmdSequencerTester ::JcfJumpsOnFailure() {
    SequenceFiles::DirectiveFile file("jcf_jumps");
    file.command(0, 1).jcf("HANDLER").exitSeq(1).label("HANDLER").command(1, 2).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // The command fails and the JCF in the very next record handles it
    this->respond(0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CS_CommandError(0, fileName, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CS_DirectiveJump(0, fileName, DirectiveId::JCF, "HANDLER");
    // A handled failure is not a cancellation
    ASSERT_EVENTS_CS_SequenceCanceled_SIZE(0);
    ASSERT_from_seqDone_SIZE(0);
    // Execution resumed at the record after the label
    this->assertCommandOut(1, 2);

    this->respond(1, Fw::CmdResponse::OK);
    ASSERT_EVENTS_CS_SequenceComplete_SIZE(1);
    ASSERT_from_seqDone_SIZE(1);
    ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::OK));
}

void CmdSequencerTester ::JcfFallsThroughOnSuccess() {
    SequenceFiles::DirectiveFile file("jcf_falls_through");
    file.command(0, 1).jcf("HANDLER").exitSeq(0).label("HANDLER").command(1, 2).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // The command succeeded, so the JCF does not jump and the EXIT after it runs
    this->respond(0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);
    ASSERT_EVENTS_CS_SequenceComplete(0, fileName);
    ASSERT_EVENTS_CS_DirectiveJump_SIZE(0);
    // The handler command after the label was never reached
    ASSERT_from_comCmdOut_SIZE(0);
    ASSERT_from_seqDone_SIZE(1);
    ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::OK));
}

void CmdSequencerTester ::JcsJumpsOnSuccess() {
    SequenceFiles::DirectiveFile file("jcs_jumps");
    file.command(0, 1).jcs("OK_PATH").exitSeq(1).label("OK_PATH").command(1, 2).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    this->respond(0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);
    ASSERT_EVENTS_CS_DirectiveJump(0, fileName, DirectiveId::JCS, "OK_PATH");
    this->assertCommandOut(1, 2);

    this->respond(1, Fw::CmdResponse::OK);
    ASSERT_from_seqDone_SIZE(1);
    ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::OK));
}

void CmdSequencerTester ::ErrorModeOffContinues() {
    SequenceFiles::DirectiveFile file("error_mode_off");
    file.errorMode(0).command(0, 1).command(1, 2).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    ASSERT_FALSE(this->component.m_errorMode);
    this->assertCommandOut(0, 1);

    // With error mode OFF the failure is reported and the sequence keeps going
    this->respond(0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_CS_CommandError(0, fileName, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_from_seqDone_SIZE(0);
    ASSERT_EQ(CmdSequencerComponentImpl::RUNNING, this->component.m_runMode);
    this->assertCommandOut(1, 2);

    this->respond(1, Fw::CmdResponse::OK);
    ASSERT_EVENTS_CS_SequenceComplete_SIZE(1);
    ASSERT_from_seqDone_SIZE(1);
    ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::OK));
    // Error mode returns to its default at the end of the sequence
    ASSERT_TRUE(this->component.m_errorMode);
}

// ----------------------------------------------------------------------
// Tests: directive interaction with error mode
// ----------------------------------------------------------------------

void CmdSequencerTester ::ErrorModeOnAbortsAtNonJcfDirective() {
    SequenceFiles::DirectiveFile file("abort_at_label");
    file.command(0, 1).label("X").command(1, 2).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // The record after the failed command is a LABEL, not a JCF, so the failure is not
    // handled and the sequence aborts
    this->respond(0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_CS_CommandError(0, fileName, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_from_comCmdOut_SIZE(0);
    this->assertAborted();
}

void CmdSequencerTester ::ErrorModeOnAbortsBeforeLaterJcf() {
    SequenceFiles::DirectiveFile file("abort_before_jcf");
    file.command(0, 1)
        .jcs("S")
        .jcf("F")
        .label("S")
        .exitSeq(0)
        .label("F")
        .command(1, 2)
        .endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // The JCF is one record too late: with error mode ON the JCS in between aborts first
    this->respond(0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_CS_CommandError(0, fileName, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CS_DirectiveJump_SIZE(0);
    ASSERT_from_comCmdOut_SIZE(0);
    this->assertAborted();
}

void CmdSequencerTester ::ConsecutiveJumpsSeeSameCommand() {
    SequenceFiles::DirectiveFile file("consecutive_jumps");
    file.errorMode(0)
        .command(0, 1)
        .jcs("S")
        .jcf("F")
        .label("S")
        .exitSeq(0)
        .label("F")
        .command(1, 2)
        .endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // Error mode is OFF, so the failure does not abort. The JCS tests the command and
    // falls through without consuming its status; the JCF then tests the same command
    // and jumps.
    this->respond(0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CS_CommandError(0, fileName, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CS_DirectiveJump(0, fileName, DirectiveId::JCF, "F");
    this->assertCommandOut(1, 2);

    this->respond(1, Fw::CmdResponse::OK);
    ASSERT_EVENTS_CS_SequenceComplete_SIZE(1);
    ASSERT_from_seqDone_SIZE(1);
    ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::OK));
}

void CmdSequencerTester ::PendingAbortDoesNotLeakToNextSequence() {
    // A sequence may legally end on a command record. With error mode ON, a failure there
    // has to be reported by the command response handler, because no directive can follow
    // to handle it.
    SequenceFiles::DirectiveFile failing("abort_on_last_record");
    failing.command(0, 1);
    const char* const failingName = this->prepare(failing);

    this->runSequence(0, failingName);
    this->assertCommandOut(0, 1);
    this->respond(0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_CS_CommandError(0, failingName, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
    this->assertAborted();
    ASSERT_FALSE(this->component.m_errorPendingAbort);

    // A second sequence must start clean: a pending abort left over from the first would
    // stop this one at its first non-directive record, with no event explaining why
    SequenceFiles::DirectiveFile clean("clean_after_abort");
    clean.command(0, 1).endOfSequence();
    const char* const cleanName = this->prepare(clean);

    this->runSequence(0, cleanName);
    this->assertCommandOut(0, 1);
    this->respond(0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CS_CommandComplete(0, cleanName, 0, 0);
    ASSERT_EVENTS_CS_SequenceComplete(0, cleanName);
    ASSERT_from_seqDone_SIZE(1);
    ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::OK));
}

void CmdSequencerTester ::PendingAbortOnLastRecordManual() {
    SequenceFiles::DirectiveFile file("abort_last_manual");
    file.command(0, 1);
    const char* const fileName = this->prepare(file);

    this->goToManualMode(10);
    this->runSequence(0, fileName);
    ASSERT_from_comCmdOut_SIZE(0);
    this->startSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // Manual mode takes the same decision as auto mode at the last record
    this->respond(0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_CS_CommandError(0, fileName, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
    this->assertAborted();
}

// ----------------------------------------------------------------------
// Tests: jump bookkeeping
// ----------------------------------------------------------------------

void CmdSequencerTester ::JumpConsumesCommandStatus() {
    SequenceFiles::DirectiveFile file("jump_consumes_status");
    file.command(0, 1).jcf("R").exitSeq(0).label("R").jcf("R").endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // The first JCF jumps. The JCF at the target has no command of its own to test, so it
    // fails rather than jumping again; without that, the pair would retry forever.
    this->respond(0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SIZE(3);
    ASSERT_EVENTS_CS_CommandError(0, fileName, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CS_DirectiveJump_SIZE(1);
    ASSERT_EVENTS_CS_DirectiveJump(0, fileName, DirectiveId::JCF, "R");
    ASSERT_EVENTS_CS_DirectiveError(0, fileName, 1, DirectiveStatus::ERROR_NO_PRIOR_COMMAND);
    this->assertAborted();
}

void CmdSequencerTester ::DirectiveCycleDetected() {
    // Four records, and a step that reads five: the ERROR_MODE record between the label and
    // the jump is read twice, which is what makes the total exceed the record count.
    SequenceFiles::DirectiveFile file("directive_cycle");
    file.command(0, 1).label("L").errorMode(1).jcs("L");
    const char* const fileName = this->prepare(file);
    ASSERT_EQ(4U, file.getNumRecords());

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    this->respond(0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(3);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);
    ASSERT_EVENTS_CS_DirectiveJump(0, fileName, DirectiveId::JCS, "L");
    ASSERT_EVENTS_CS_DirectiveError(0, fileName, 1, DirectiveStatus::ERROR_DIRECTIVE_CYCLE);
    ASSERT_from_comCmdOut_SIZE(0);
    this->assertAborted();
}

void CmdSequencerTester ::MalformedLabelSkippedDuringSearch() {
    SequenceFiles::DirectiveFile file("bad_label_search");
    file.command(0, 1)
        .jcs("GOOD")
        .jumpWithLabelLength(DirectiveId::LABEL, 21, OVERLONG_LABEL)
        .label("GOOD")
        .command(1, 2)
        .endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // The search passes over the malformed LABEL at record 2, reports it, and goes on to
    // find the real label at record 3
    this->respond(0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(3);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);
    ASSERT_EVENTS_CS_LabelRecordInvalid(0, fileName, 2, static_cast<I32>(Fw::FW_DESERIALIZE_SIZE_MISMATCH));
    ASSERT_EVENTS_CS_DirectiveJump(0, fileName, DirectiveId::JCS, "GOOD");
    this->assertCommandOut(1, 2);
}

void CmdSequencerTester ::UnknownDirectiveSkippedDuringSearch() {
    const U8 unknownId = 99;
    SequenceFiles::DirectiveFile file("bad_directive_search");
    file.command(0, 1).jcs("GOOD").directiveIdOnly(unknownId).label("GOOD").command(1, 2).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // The unknown directive is never executed, but it is not silently discarded either
    this->respond(0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(3);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);
    ASSERT_EVENTS_CS_LabelRecordInvalid(0, fileName, 2, static_cast<I32>(unknownId));
    ASSERT_EVENTS_CS_DirectiveJump(0, fileName, DirectiveId::JCS, "GOOD");
    this->assertCommandOut(1, 2);
}

// ----------------------------------------------------------------------
// Tests: malformed and invalid directives
// ----------------------------------------------------------------------

void CmdSequencerTester ::EmptyDirectiveRecord() {
    SequenceFiles::DirectiveFile file("empty_directive");
    file.emptyDirective().endOfSequence();
    const char* const fileName = this->prepare(file);

    // The first step reads the directive and fails, so the run command reports failure
    this->sendRun(fileName, Svc::BlockState::NO_BLOCK);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, CmdSequencerComponentBase::OPCODE_CS_RUN, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CS_DirectiveError_SIZE(1);
    ASSERT_EVENTS_CS_DirectiveError(0, fileName, 0, DirectiveStatus::ERROR_MALFORMED_RECORD);
    this->assertAborted();
}

void CmdSequencerTester ::UnknownDirectiveId() {
    SequenceFiles::DirectiveFile file("unknown_directive_id");
    file.directiveIdOnly(99).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->sendRun(fileName, Svc::BlockState::NO_BLOCK);
    ASSERT_CMD_RESPONSE(0, CmdSequencerComponentBase::OPCODE_CS_RUN, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CS_DirectiveError_SIZE(1);
    ASSERT_EVENTS_CS_DirectiveError(0, fileName, 0, DirectiveStatus::ERROR_INVALID_ARGUMENT);
    this->assertAborted();
}

void CmdSequencerTester ::ExitWithNoArgument() {
    SequenceFiles::DirectiveFile file("exit_no_argument");
    file.directiveIdOnly(DirectiveId::EXIT).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->sendRun(fileName, Svc::BlockState::NO_BLOCK);
    ASSERT_CMD_RESPONSE(0, CmdSequencerComponentBase::OPCODE_CS_RUN, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CS_DirectiveError_SIZE(1);
    ASSERT_EVENTS_CS_DirectiveError(0, fileName, 0, DirectiveStatus::ERROR_MALFORMED_RECORD);
    this->assertAborted();
}

void CmdSequencerTester ::ExitWithInvalidStatus() {
    SequenceFiles::DirectiveFile file("exit_invalid_status");
    file.exitSeq(5).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->sendRun(fileName, Svc::BlockState::NO_BLOCK);
    ASSERT_CMD_RESPONSE(0, CmdSequencerComponentBase::OPCODE_CS_RUN, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CS_DirectiveError_SIZE(1);
    ASSERT_EVENTS_CS_DirectiveError(0, fileName, 0, DirectiveStatus::ERROR_INVALID_ARGUMENT);
    this->assertAborted();
}

void CmdSequencerTester ::ErrorModeWithInvalidArgument() {
    SequenceFiles::DirectiveFile file("error_mode_invalid");
    file.errorMode(7).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->sendRun(fileName, Svc::BlockState::NO_BLOCK);
    ASSERT_CMD_RESPONSE(0, CmdSequencerComponentBase::OPCODE_CS_RUN, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CS_DirectiveError_SIZE(1);
    ASSERT_EVENTS_CS_DirectiveError(0, fileName, 0, DirectiveStatus::ERROR_INVALID_ARGUMENT);
    this->assertAborted();
    // The invalid argument did not change the mode
    ASSERT_TRUE(this->component.m_errorMode);
}

void CmdSequencerTester ::JumpWithOverlongLabel() {
    SequenceFiles::DirectiveFile file("jump_overlong_label");
    file.command(0, 1).jumpWithLabelLength(DirectiveId::JCS, 21, OVERLONG_LABEL).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // The declared length exceeds what the record format can express, so the label is
    // never copied out
    this->respond(0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);
    ASSERT_EVENTS_CS_DirectiveError(0, fileName, 1, DirectiveStatus::ERROR_MALFORMED_RECORD);
    this->assertAborted();
}

void CmdSequencerTester ::JumpWithTruncatedLabel() {
    SequenceFiles::DirectiveFile file("jump_truncated_label");
    // Ten characters are claimed and two are present
    file.command(0, 1).jumpWithLabelLength(DirectiveId::JCS, 10, "AB").endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    this->respond(0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);
    ASSERT_EVENTS_CS_DirectiveError(0, fileName, 1, DirectiveStatus::ERROR_MALFORMED_RECORD);
    this->assertAborted();
}

void CmdSequencerTester ::JumpWithNoPriorCommand() {
    SequenceFiles::DirectiveFile file("jump_no_prior_command");
    file.jcf("X").label("X").command(0, 1).endOfSequence();
    const char* const fileName = this->prepare(file);

    // The JCF is the first record, so there is no command status for it to test
    this->sendRun(fileName, Svc::BlockState::NO_BLOCK);
    ASSERT_CMD_RESPONSE(0, CmdSequencerComponentBase::OPCODE_CS_RUN, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CS_DirectiveError_SIZE(1);
    ASSERT_EVENTS_CS_DirectiveError(0, fileName, 0, DirectiveStatus::ERROR_NO_PRIOR_COMMAND);
    ASSERT_from_comCmdOut_SIZE(0);
    this->assertAborted();
}

void CmdSequencerTester ::JumpToMissingLabel() {
    SequenceFiles::DirectiveFile file("jump_missing_label");
    file.command(0, 1).jcs("MISSING").endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    this->respond(0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);
    ASSERT_EVENTS_CS_DirectiveError(0, fileName, 1, DirectiveStatus::ERROR_LABEL_NOT_FOUND);
    ASSERT_EVENTS_CS_LabelRecordInvalid_SIZE(0);
    this->assertAborted();
}

// ----------------------------------------------------------------------
// Tests: manual mode and command responses
// ----------------------------------------------------------------------

void CmdSequencerTester ::ManualStepConsumesOneDirective() {
    SequenceFiles::DirectiveFile file("manual_step_directive");
    file.command(0, 1).jcf("H").exitSeq(1).label("H").command(1, 2).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->goToManualMode(10);
    this->runSequence(0, fileName);
    ASSERT_from_comCmdOut_SIZE(0);
    this->startSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // The failure waits for the next step in manual mode
    this->respond(0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_CS_CommandError(0, fileName, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EQ(CmdSequencerComponentImpl::RUNNING, this->component.m_runMode);

    // The first step consumes only the JCF, not the command it jumped to
    this->step(Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CS_DirectiveJump(0, fileName, DirectiveId::JCF, "H");
    ASSERT_EVENTS_CS_CmdStepped(0, fileName, 1);
    ASSERT_from_comCmdOut_SIZE(0);

    // The second step issues the handler command
    this->step(Fw::CmdResponse::OK);
    this->assertCommandOut(1, 2);
    ASSERT_EVENTS_CS_CmdStepped_SIZE(1);

    this->respond(1, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 1, 1);

    // The last step reaches the end-of-sequence record
    this->step(Fw::CmdResponse::OK);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_CS_SequenceComplete(0, fileName);
    ASSERT_from_seqDone_SIZE(1);
    ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::OK));
}

void CmdSequencerTester ::ManualStepResponseDistinguishesEndFromAbort() {
    // An orderly end of sequence stops the sequencer, and the step that reached it
    // succeeded
    {
        SequenceFiles::DirectiveFile file("manual_step_end");
        file.command(0, 1).endOfSequence();
        const char* const fileName = this->prepare(file);

        this->goToManualMode(10);
        this->runSequence(0, fileName);
        this->startSequence(0, fileName);
        this->assertCommandOut(0, 1);
        this->respond(0, Fw::CmdResponse::OK);
        ASSERT_EVENTS_SIZE(1);
        ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);

        this->step(Fw::CmdResponse::OK);
        ASSERT_EVENTS_SIZE(1);
        ASSERT_EVENTS_CS_SequenceComplete(0, fileName);
        // The step that ended the sequence is not itself reported as a step
        ASSERT_EVENTS_CS_CmdStepped_SIZE(0);
        ASSERT_EQ(CmdSequencerComponentImpl::STOPPED, this->component.m_runMode);
        this->goToAutoMode(11);
    }
    // An abort also stops the sequencer, but the step must not be acknowledged as OK
    {
        SequenceFiles::DirectiveFile file("manual_step_abort");
        file.command(0, 1).label("X").command(1, 2).endOfSequence();
        const char* const fileName = this->prepare(file);

        this->goToManualMode(12);
        this->runSequence(0, fileName);
        this->startSequence(0, fileName);
        this->assertCommandOut(0, 1);
        this->respond(0, Fw::CmdResponse::EXECUTION_ERROR);
        ASSERT_EVENTS_SIZE(1);
        ASSERT_EVENTS_CS_CommandError(0, fileName, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);

        this->step(Fw::CmdResponse::EXECUTION_ERROR);
        ASSERT_EVENTS_CS_CmdStepped_SIZE(0);
        ASSERT_from_comCmdOut_SIZE(0);
        this->assertAborted();
    }
}

void CmdSequencerTester ::FailedCommandAdvancesRecordIndex() {
    SequenceFiles::DirectiveFile file("failed_command_index");
    file.errorMode(0).command(0, 1).command(1, 2).endOfSequence();
    const char* const fileName = this->prepare(file);

    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);

    // The failed command is record 0
    this->respond(0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CS_CommandError(0, fileName, 0, 0, Fw::CmdResponse::EXECUTION_ERROR);
    this->assertCommandOut(1, 2);

    // The command after it is record 1, not record 0 again
    this->respond(1, Fw::CmdResponse::OK);
    ASSERT_EVENTS_CS_CommandComplete(0, fileName, 1, 1);
    // Only the command that succeeded is counted as executed
    ASSERT_TLM_CS_CommandsExecuted(0, 1);
}

void CmdSequencerTester ::BlockingRunAnsweredOnceWhenSequenceEndsInFirstStep() {
    SequenceFiles::DirectiveFile file("blocking_run_exit");
    file.exitSeq(0);
    const char* const fileName = this->prepare(file);

    // The EXIT directive ends the sequence inside the first step, which answers the
    // blocking caller and clears the block state. The run handler must not answer again.
    this->sendRun(fileName, Svc::BlockState::BLOCK);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, CmdSequencerComponentBase::OPCODE_CS_RUN, 0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_CS_SequenceComplete_SIZE(1);
    ASSERT_from_seqDone_SIZE(1);
    ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::OK));
}

void CmdSequencerTester ::InvalidModeNamesItsCause() {
    SequenceFiles::DirectiveFile file("invalid_mode_cause");
    file.command(0, 1).endOfSequence();
    const char* const fileName = this->prepare(file);

    // A blocking run cannot be answered in manual mode, because nothing runs until a step
    this->goToManualMode(10);
    this->sendRun(fileName, Svc::BlockState::BLOCK);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, CmdSequencerComponentBase::OPCODE_CS_RUN, 0, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_CS_InvalidMode(0, InvalidModeCause::RUN_BLOCK_IN_MANUAL);

    // A step is meaningless in auto mode
    this->goToAutoMode(11);
    this->runSequence(0, fileName);
    this->assertCommandOut(0, 1);
    this->sendCmd_CS_STEP(0, 12);
    this->clearAndDispatch();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, CmdSequencerComponentBase::OPCODE_CS_STEP, 12, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_CS_InvalidMode(0, InvalidModeCause::STEP_NOT_MANUAL);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

const char* CmdSequencerTester ::prepare(SequenceFiles::DirectiveFile& file) {
    // A zero time makes every relative command record immediate, so no test has to advance
    // the clock to get a command dispatched
    const Fw::Time testTime(TimeBase::TB_WORKSTATION_TIME, 0, 0);
    this->setTestTime(testTime);
    const char* const fileName = file.getName().toChar();
    file.write();
    this->validateFile(0, fileName);
    return fileName;
}

void CmdSequencerTester ::sendRun(const char* const fileName, const Svc::BlockState::t block) {
    this->sendCmd_CS_RUN(0, 0, Fw::CmdStringArg(fileName), Svc::BlockState(block));
    this->clearAndDispatch();
}

void CmdSequencerTester ::assertCommandOut(const FwOpcodeType opcode, const U32 argument) {
    Fw::ComBuffer comBuff;
    CommandBuffers::create(comBuff, opcode, argument);
    ASSERT_from_comCmdOut_SIZE(1);
    ASSERT_from_comCmdOut(0, comBuff, 0U);
}

void CmdSequencerTester ::respond(const FwOpcodeType opcode, const Fw::CmdResponse& response) {
    this->invoke_to_cmdResponseIn(0, opcode, 0, response);
    this->clearAndDispatch();
}

void CmdSequencerTester ::step(const Fw::CmdResponse& expected) {
    this->sendCmd_CS_STEP(0, 0);
    this->clearAndDispatch();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, CmdSequencerComponentBase::OPCODE_CS_STEP, 0, expected);
}

void CmdSequencerTester ::assertAborted() {
    ASSERT_EQ(CmdSequencerComponentImpl::STOPPED, this->component.m_runMode);
    // One abort, not two: the cancel is issued by whichever handler owns the step
    ASSERT_from_seqDone_SIZE(1);
    ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::EXECUTION_ERROR));
}

}  // namespace Directives

}  // namespace Svc
