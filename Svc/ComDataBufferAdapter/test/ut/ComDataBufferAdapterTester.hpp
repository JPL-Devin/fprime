// ======================================================================
// \title  ComDataBufferAdapterTester.hpp
// \brief  hpp file for ComDataBufferAdapter component test harness implementation class
// ======================================================================

#ifndef Svc_ComDataBufferAdapterTester_HPP
#define Svc_ComDataBufferAdapterTester_HPP

#include "Svc/ComDataBufferAdapter/ComDataBufferAdapter.hpp"
#include "Svc/ComDataBufferAdapter/ComDataBufferAdapterGTestBase.hpp"

namespace Svc {

class ComDataBufferAdapterTester final : public ComDataBufferAdapterGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object ComDataBufferAdapterTester
    ComDataBufferAdapterTester();

    //! Destroy object ComDataBufferAdapterTester
    ~ComDataBufferAdapterTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! bufferIn is forwarded on dataOut with the default context
    void testSendDefaultContext();

    //! bufferIn is forwarded on dataOut with the configured context
    void testSendConfiguredContext();

    //! dataReturnIn is forwarded on bufferInReturn
    void testSendReturn();

    //! dataIn is forwarded on bufferOut
    void testReceive();

    //! bufferOutReturn is forwarded on dataReturnOut
    void testReceiveReturn();

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Output ports of the component under test
    enum class OutputPort { DATA_OUT, BUFFER_IN_RETURN, BUFFER_OUT, DATA_RETURN_OUT };

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Clear history and invoke bufferIn
    void sendBufferIn(Fw::Buffer& buffer);

    //! Clear history and invoke dataReturnIn
    void sendDataReturnIn(Fw::Buffer& buffer, const ComCfg::FrameContext& context);

    //! Clear history and invoke dataIn
    void sendDataIn(Fw::Buffer& buffer, const ComCfg::FrameContext& context);

    //! Clear history and invoke bufferOutReturn
    void sendBufferOutReturn(Fw::Buffer& buffer);

    //! Assert exactly one dataOut call carrying buffer and context, and no other output
    void assertDataOut(const Fw::Buffer& buffer, const ComCfg::FrameContext& context);

    //! Assert exactly one bufferInReturn call carrying buffer, and no other output
    void assertBufferInReturn(const Fw::Buffer& buffer);

    //! Assert exactly one bufferOut call carrying buffer, and no other output
    void assertBufferOut(const Fw::Buffer& buffer);

    //! Assert exactly one dataReturnOut call carrying buffer with default context, and no other output
    void assertDataReturnOut(const Fw::Buffer& buffer);

    //! Assert that no output port other than except was invoked
    void assertNoOtherOutput(OutputPort except);

    //! Assert that a buffer refers to the same data as the original (no copy)
    static void assertSameBuffer(const Fw::Buffer& expected, const Fw::Buffer& actual);

    //! Build a buffer of random size over m_data filled with random data
    Fw::Buffer randomBuffer();

    //! Build a frame context with random field values
    static ComCfg::FrameContext randomContext();

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    ComDataBufferAdapter component;

    //! Backing storage for test buffers
    U8 m_data[FW_COM_BUFFER_MAX_SIZE];
};

}  // namespace Svc

#endif
