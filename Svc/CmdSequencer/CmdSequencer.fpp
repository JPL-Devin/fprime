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

    @ The result of executing a sequence directive
    enum DirectiveStatus : U8 {
      CONTINUE = 0               @< The directive completed; advance to the next record
      JUMPED = 1                 @< A jump was taken; resume at the jump target
      SEQUENCE_ENDED = 2         @< The directive ended the sequence; completion is already reported
      ABORT_PENDING_ERROR = 3    @< A prior command failed with error mode ON and this directive does not handle it
      ERROR_MALFORMED_RECORD = 4 @< The directive payload could not be deserialized
      ERROR_INVALID_ARGUMENT = 5 @< A directive argument was outside its permitted range
      ERROR_NO_PRIOR_COMMAND = 6 @< A JCF or JCS directive ran before any command executed
      ERROR_LABEL_NOT_FOUND = 7  @< The jump target label is not present in the sequence
      ERROR_DIRECTIVE_CYCLE = 8  @< Directives redirected execution in a cycle with no command between
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
