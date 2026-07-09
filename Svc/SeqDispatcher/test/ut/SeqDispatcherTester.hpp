// ======================================================================
// \title  SeqDispatcher/test/ut/Tester.hpp
// \author zimri.leisher
// \brief  hpp file for SeqDispatcher test harness implementation class
// ======================================================================

#ifndef TESTER_HPP
#define TESTER_HPP

#include "SeqDispatcherGTestBase.hpp"
#include "Svc/SeqDispatcher/SeqDispatcher.hpp"
#include "Svc/SeqDispatcher/test/ut/TestState/TestState.hpp"
#include "TestUtils/RuleBasedTesting.hpp"

namespace Svc {

class SeqDispatcherTester : public SeqDispatcherGTestBase {
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

  public:
    // Maximum size of histories storing events, telemetry, and port outputs
    static const U32 MAX_HISTORY_SIZE = 100;
    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;
    // Queue depth supplied to component instance under test
    static const FwSizeType TEST_INSTANCE_QUEUE_DEPTH = 10;

    //! Construct object SeqDispatcherTester
    //!
    SeqDispatcherTester();

    //! Destroy object SeqDispatcherTester
    //!
    ~SeqDispatcherTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    void testDispatch();
    void testLogStatus();
    void testRunArgsWithValidArguments();
    void testRunArgsWithMaxSizedArguments();
    void testRunArgsNoSequencersAvailable();
    void testRunArgsBlockingVsNonBlocking();

  private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports
    // ----------------------------------------------------------------------

    void seqRunOut_handler(FwIndexType portNum,             //!< The port number
                           const Fw::StringBase& filename,  //!< The sequence file
                           const Svc::SeqArgs& args         //!< Sequence arguments
    );

  private:
    // ----------------------------------------------------------------------
    // Helper methods
    // ----------------------------------------------------------------------

    //! Connect ports
    //!
    void connectPorts();

    //! Initialize components
    //!
    void initComponents();

  public:
    // ----------------------------------------------------------------------
    // Variables
    // ----------------------------------------------------------------------

    //! The component under test
    //!
    SeqDispatcher component;

    //! Shadow state for rule-based testing
    SeqDispatcherTestState shadow;

  public:
    // ----------------------------------------------------------------------
    // Rule Based Testing
    // ----------------------------------------------------------------------

    //! Rules for the RUN command
    FW_RBT_DEFINE_RULE(SeqDispatcherTester, RunCmd, Ok);
    FW_RBT_DEFINE_RULE(SeqDispatcherTester, RunCmd, NoSequencersAvailable);

    //! Rules for the seqDoneIn port
    FW_RBT_DEFINE_RULE(SeqDispatcherTester, SeqDone, KnownBlock);
    FW_RBT_DEFINE_RULE(SeqDispatcherTester, SeqDone, KnownNoBlock);
    FW_RBT_DEFINE_RULE(SeqDispatcherTester, SeqDone, Unknown);

    //! Rules for the seqStartIn port
    FW_RBT_DEFINE_RULE(SeqDispatcherTester, SeqStart, Expected);
    FW_RBT_DEFINE_RULE(SeqDispatcherTester, SeqStart, Unexpected);
    FW_RBT_DEFINE_RULE(SeqDispatcherTester, SeqStart, Conflicting);

    //! Rules for the seqRunIn port
    FW_RBT_DEFINE_RULE(SeqDispatcherTester, SeqRunIn, Ok);
    FW_RBT_DEFINE_RULE(SeqDispatcherTester, SeqRunIn, NoSequencersAvailable);

    //! Rules for the LOG_STATUS command
    FW_RBT_DEFINE_RULE(SeqDispatcherTester, LogStatusCmd, Ok);
};

}  // namespace Svc

#endif
