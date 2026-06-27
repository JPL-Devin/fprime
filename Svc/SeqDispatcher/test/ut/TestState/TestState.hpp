// ======================================================================
// \title  TestState.hpp
// \brief  Shadow state model for SeqDispatcher rule-based testing
//
// SeqDispatcherTestState contains shadow-model data and state-only helpers.
// Rule preconditions/actions are implemented on SeqDispatcherTester.
// ======================================================================

#ifndef Svc_SeqDispatcher_TestState_HPP
#define Svc_SeqDispatcher_TestState_HPP

#include "Fw/Types/String.hpp"
#include "Svc/SeqDispatcher/SeqDispatcher_CmdSequencerStateEnumAc.hpp"
#include "config/FppConstantsAc.hpp"

namespace Svc {

class SeqDispatcherTestState {
  public:
    struct SequencerEntry {
        SeqDispatcher_CmdSequencerState state = SeqDispatcher_CmdSequencerState::AVAILABLE;
        Fw::String sequenceRunning = "<no seq>";
        FwOpcodeType opCode = 0;
        U32 cmdSeq = 0;
    };

    SequencerEntry shadow_entryTable[SeqDispatcherSequencerPorts];
    U32 shadow_sequencersAvailable = SeqDispatcherSequencerPorts;
    U32 shadow_dispatchedCount = 0;
    U32 shadow_errorCount = 0;

  public:
    //! Return index of first available sequencer, or -1 if none
    FwIndexType shadow_getNextAvailableIdx() const;

    //! Return true if any sequencer is available
    bool shadow_hasAvailableSequencer() const;

    //! Return true if any sequencer is running a blocking sequence
    bool shadow_hasBlockingSequencer() const;

    //! Return true if any sequencer is running a non-blocking sequence
    bool shadow_hasNonBlockingSequencer() const;

    //! Return true if any sequencer is running (block or no-block)
    bool shadow_hasRunningSequencer() const;

    //! Return index of a random sequencer in the given state, or -1 if none
    FwIndexType shadow_getRandomSequencerInState(SeqDispatcher_CmdSequencerState state) const;
};

}  // namespace Svc

#endif
