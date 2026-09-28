module Svc {

  @ A component for running command sequences
  active component CmdSequencer {

    # ----------------------------------------------------------------------
    # Types
    # ----------------------------------------------------------------------

    @ The sequencer mode
    enum SeqMode : U8 {
      STEP = 0
      AUTO = 1
    }

    @ The stage of the file read operation
    enum FileReadStage : U8 {
      READ_HEADER
      READ_HEADER_SIZE
      DESER_SIZE
      DESER_NUM_RECORDS
      DESER_TIME_BASE
      DESER_TIME_CONTEXT
      READ_SEQ_CRC
      READ_SEQ_DATA
      READ_SEQ_DATA_SIZE
    }

    @ Identifies a sequence directive. Held in the first byte of the record payload of
    @ a record whose descriptor is 3 (sequence directive).
    enum DirectiveId : U8 {
      LABEL = 0      @< Marks a jump target
      JCF = 1        @< Jump Command Failure: jump to a label if the preceding command failed
      EXIT = 2       @< End the sequence with a specified status
      JCS = 3        @< Jump Command Success: jump to a label if the preceding command succeeded
      ERROR_MODE = 4 @< Control whether a command failure aborts the sequence
    }

    @ Identifies the condition that produced an invalid-mode report. Reported with
    @ CS_InvalidMode so that each emission of that event is traceable to one site.
    enum InvalidModeCause : U8 {
      RUN_NOT_STOPPED = 0             @< CS_RUN arrived while a sequence was running
      RUN_BLOCK_IN_MANUAL = 1         @< CS_RUN requested BLOCK while in MANUAL step mode
      VALIDATE_NOT_STOPPED = 2        @< CS_VALIDATE arrived while a sequence was running
      PORT_RUN_NOT_STOPPED = 3        @< A port-driven run arrived while a sequence was running
      PORT_RUN_IN_MANUAL = 4          @< A port-driven run arrived while in MANUAL step mode
      START_NOT_STOPPED = 5           @< CS_START arrived while a sequence was running
      STEP_NOT_RUNNING = 6            @< CS_STEP arrived with no sequence running
      STEP_NOT_MANUAL = 7             @< CS_STEP arrived while in AUTO step mode
      AUTO_NOT_STOPPED = 8            @< CS_AUTO arrived while a sequence was running
      MANUAL_NOT_STOPPED = 9          @< CS_MANUAL arrived while a sequence was running
    }

    @ Why a sequence directive failed. Each value names the check that rejected the
    @ directive, so that a CS_DirectiveError or CS_LabelRecordInvalid report is traceable
    @ to one field of one directive.
    enum DirectiveError : U8 {
      DIRECTIVE_ID_UNREADABLE = 0 @< The record payload is too short to hold a directive ID
      UNKNOWN_DIRECTIVE = 1       @< The directive ID is not a defined DirectiveId
      LABEL_UNREADABLE = 2        @< The label of a LABEL, JCF, or JCS is missing, truncated, or longer than 20 characters
      EXIT_STATUS_UNREADABLE = 3  @< The status argument of EXIT is missing
      EXIT_STATUS_INVALID = 4     @< The status argument of EXIT is not 0 or 1
      ERROR_MODE_UNREADABLE = 5   @< The mode argument of ERROR_MODE is missing
      ERROR_MODE_INVALID = 6      @< The mode argument of ERROR_MODE is not 0 or 1
      NO_PRIOR_COMMAND = 7        @< A JCF or JCS ran before any command completed
      LABEL_NOT_FOUND = 8         @< The jump target of a JCF or JCS is not in the sequence
      DIRECTIVE_CYCLE = 9         @< Directives redirected execution in a cycle with no command between
    }

    # ----------------------------------------------------------------------
    # Special ports
    # ----------------------------------------------------------------------

    @ Command receive port
    command recv port cmdIn

    @ Command registration port
    command reg port cmdRegOut

    @ Command response port
    command resp port cmdResponseOut

    @ Event port
    event port logOut

    @ Telemetry port
    telemetry port tlmOut

    @ Text event port
    text event port LogText

    @ Time get port
    time get port timeCaller

    # ----------------------------------------------------------------------
    # General ports
    # ----------------------------------------------------------------------

    @ Sequence cancel port
    async input port seqCancelIn: Svc.CmdSeqCancel

    @ Command response in port
    async input port cmdResponseIn: Fw.CmdResponse

    @ Ping in port
    async input port pingIn: Svc.Ping drop

    @ Ping out port
    output port pingOut: Svc.Ping

    @ Port for indicating sequence done
    output port seqDone: Fw.CmdResponse

    @ Port for requests to run sequences
    async input port seqRunIn: Svc.CmdSeqIn

    @ Port for file dispatches to run sequences
    async input port seqDispatchIn: Svc.FileDispatch

    @ Port for sending sequence commands
    output port comCmdOut: Fw.Com

    @ Schedule in port
    async input port schedIn: Svc.Sched

    @ Notifies that a sequence has started running
    output port seqStartOut: Svc.CmdSeqIn

    # ----------------------------------------------------------------------
    # Commands
    # ----------------------------------------------------------------------

    include "Commands.fppi"

    # ----------------------------------------------------------------------
    # Telemetry
    # ----------------------------------------------------------------------

    include "Telemetry.fppi"

    # ----------------------------------------------------------------------
    # Events
    # ----------------------------------------------------------------------

    include "Events.fppi"

  }

}
