// ======================================================================
// \title  Directives.hpp
// \author Canham
// \brief  Tests for sequence directives
//
// \copyright
// Copyright (C) 2009-2018 California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
// ======================================================================

#ifndef Svc_Directives_HPP
#define Svc_Directives_HPP

#include "Svc/CmdSequencer/test/ut/CmdSequencerTester.hpp"
#include "Svc/CmdSequencer/test/ut/SequenceFiles/DirectiveFile.hpp"

namespace Svc {

namespace Directives {

//! Tests for the sequence directives LABEL, JCF, JCS, EXIT, and ERROR_MODE
//!
//! Only the F Prime sequence format has directive records, so every test here builds a
//! SequenceFiles::DirectiveFile, which writes F Prime format only.
class CmdSequencerTester : public Svc::CmdSequencerTester {
  public:
    // ----------------------------------------------------------------------
    // Constructors
    // ----------------------------------------------------------------------

    //! Construct object CmdSequencerTester
    CmdSequencerTester(const SequenceFiles::File::Format::t a_format =
                           SequenceFiles::File::Format::F_PRIME  //!< The file format to use
    );

  public:
    // ----------------------------------------------------------------------
    // Tests: one directive at a time
    // ----------------------------------------------------------------------

    //! A LABEL record is stepped over and does not consume a CS_STEP in auto mode
    void LabelIsSkipped();

    //! An EXIT directive with status 0 ends the sequence successfully
    void ExitOk();

    //! An EXIT directive with status 1 ends the sequence with an error
    void ExitError();

    //! A JCF directive jumps when the preceding command failed
    void JcfJumpsOnFailure();

    //! A JCF directive falls through when the preceding command succeeded
    void JcfFallsThroughOnSuccess();

    //! A JCS directive jumps when the preceding command succeeded
    void JcsJumpsOnSuccess();

    //! An ERROR_MODE directive with argument 0 lets a failed command continue
    void ErrorModeOffContinues();

  public:
    // ----------------------------------------------------------------------
    // Tests: directive interaction with error mode
    // ----------------------------------------------------------------------

    //! With error mode ON, a directive that is not a JCF aborts the sequence, once
    void ErrorModeOnAbortsAtNonJcfDirective();

    //! With error mode ON, a JCS between the failed command and a JCF aborts before the
    //! JCF is reached: the abort window is the immediately following record
    void ErrorModeOnAbortsBeforeLaterJcf();

    //! With error mode OFF, a JCS and a JCF after one failed command both see that
    //! command's status
    void ConsecutiveJumpsSeeSameCommand();

    //! A sequence whose last record is a failed command aborts, and the next sequence does
    //! not inherit the pending abort
    void PendingAbortDoesNotLeakToNextSequence();

    //! In manual mode a failure on the last record aborts when the step mode is manual too
    void PendingAbortOnLastRecordManual();

  public:
    // ----------------------------------------------------------------------
    // Tests: jump bookkeeping
    // ----------------------------------------------------------------------

    //! A jump consumes the command status, so a jump target's own JCF has no command to
    //! test
    void JumpConsumesCommandStatus();

    //! Directives that jump over more records than the sequence holds without reaching a
    //! command are reported as a cycle
    void DirectiveCycleDetected();

    //! A malformed LABEL record passed over during a label search is reported and skipped
    void MalformedLabelSkippedDuringSearch();

    //! A record with an unknown directive ID passed over during a label search is reported
    //! and skipped
    void UnknownDirectiveSkippedDuringSearch();

  public:
    // ----------------------------------------------------------------------
    // Tests: malformed and invalid directives
    // ----------------------------------------------------------------------

    //! A directive record with no payload cannot yield a directive ID
    void EmptyDirectiveRecord();

    //! A directive ID outside the enumeration is rejected
    void UnknownDirectiveId();

    //! An EXIT directive with no status argument is rejected
    void ExitWithNoArgument();

    //! An EXIT directive with a status outside {0, 1} is rejected
    void ExitWithInvalidStatus();

    //! An ERROR_MODE directive with a mode outside {0, 1} is rejected
    void ErrorModeWithInvalidArgument();

    //! A jump directive whose declared label length exceeds the format maximum is rejected
    void JumpWithOverlongLabel();

    //! A jump directive whose declared label length exceeds the bytes present is rejected
    void JumpWithTruncatedLabel();

    //! A jump directive with no preceding command is rejected
    void JumpWithNoPriorCommand();

    //! A jump to a label the sequence does not contain is rejected
    void JumpToMissingLabel();

  public:
    // ----------------------------------------------------------------------
    // Tests: manual mode and command responses
    // ----------------------------------------------------------------------

    //! One CS_STEP consumes one directive, then the next CS_STEP issues the command
    void ManualStepConsumesOneDirective();

    //! CS_STEP answers OK at an orderly end of sequence and EXECUTION_ERROR on an abort
    void ManualStepResponseDistinguishesEndFromAbort();

    //! A failed command advances the record index used by later events
    void FailedCommandAdvancesRecordIndex();

    //! A CS_RUN in BLOCK mode whose sequence ends inside the first step is answered once
    void BlockingRunAnsweredOnceWhenSequenceEndsInFirstStep();

    //! CS_STEP in auto mode and CS_RUN with BLOCK in manual mode name their own cause
    void InvalidModeNamesItsCause();

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Set the test time, write the file, and validate it
    //! \return The file name, valid for the lifetime of the file
    const char* prepare(SequenceFiles::DirectiveFile& file  //!< The file
    );

    //! Send CS_RUN with the given block state and dispatch, without asserting the outcome
    void sendRun(const char* const fileName,     //!< The file name
                 const Svc::BlockState::t block  //!< The block state to request
    );

    //! Assert that exactly one command went out, carrying the given opcode and argument
    void assertCommandOut(const FwOpcodeType opcode,  //!< The expected opcode
                          const U32 argument          //!< The expected argument
    );

    //! Send a command response for the given opcode and dispatch
    void respond(const FwOpcodeType opcode,       //!< The opcode being answered
                 const Fw::CmdResponse& response  //!< The response
    );

    //! Send CS_STEP and dispatch, asserting only the command response
    void step(const Fw::CmdResponse& expected  //!< The expected response to CS_STEP
    );

    //! Assert that the sequencer is stopped and that it reported exactly one abort
    void assertAborted();
};

}  // namespace Directives

}  // namespace Svc

#endif
