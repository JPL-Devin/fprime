// ======================================================================
// \title  ComDataBufferAdapterTester.cpp
// \brief  cpp file for ComDataBufferAdapter component test harness implementation class
// ======================================================================

#include "ComDataBufferAdapterTester.hpp"
#include <STest/Pick/Pick.hpp>

namespace Svc {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

ComDataBufferAdapterTester::ComDataBufferAdapterTester()
    : ComDataBufferAdapterGTestBase("ComDataBufferAdapterTester", ComDataBufferAdapterTester::MAX_HISTORY_SIZE),
      component("ComDataBufferAdapter"),
      m_data() {
    this->initComponents();
    this->connectPorts();
}

ComDataBufferAdapterTester::~ComDataBufferAdapterTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void ComDataBufferAdapterTester::testSendDefaultContext() {
    Fw::Buffer buffer = this->randomBuffer();
    this->sendBufferIn(buffer);
    this->assertDataOut(buffer, ComCfg::FrameContext());
}

void ComDataBufferAdapterTester::testSendConfiguredContext() {
    ComCfg::FrameContext context = this->randomContext();
    this->component.configure(context);

    Fw::Buffer buffer = this->randomBuffer();
    this->sendBufferIn(buffer);
    this->assertDataOut(buffer, context);
}

void ComDataBufferAdapterTester::testSendReturn() {
    Fw::Buffer buffer = this->randomBuffer();
    this->sendDataReturnIn(buffer, this->randomContext());
    this->assertBufferInReturn(buffer);
}

void ComDataBufferAdapterTester::testReceive() {
    Fw::Buffer buffer = this->randomBuffer();
    this->sendDataIn(buffer, this->randomContext());
    this->assertBufferOut(buffer);
}

void ComDataBufferAdapterTester::testReceiveReturn() {
    Fw::Buffer buffer = this->randomBuffer();
    this->sendBufferOutReturn(buffer);
    this->assertDataReturnOut(buffer);
}

// ----------------------------------------------------------------------
// Helper functions: port invocations
// ----------------------------------------------------------------------

void ComDataBufferAdapterTester::sendBufferIn(Fw::Buffer& buffer) {
    this->clearHistory();
    this->invoke_to_bufferIn(0, buffer);
}

void ComDataBufferAdapterTester::sendDataReturnIn(Fw::Buffer& buffer, const ComCfg::FrameContext& context) {
    this->clearHistory();
    this->invoke_to_dataReturnIn(0, buffer, context);
}

void ComDataBufferAdapterTester::sendDataIn(Fw::Buffer& buffer, const ComCfg::FrameContext& context) {
    this->clearHistory();
    this->invoke_to_dataIn(0, buffer, context);
}

void ComDataBufferAdapterTester::sendBufferOutReturn(Fw::Buffer& buffer) {
    this->clearHistory();
    this->invoke_to_bufferOutReturn(0, buffer);
}

// ----------------------------------------------------------------------
// Helper functions: assertions
// ----------------------------------------------------------------------

void ComDataBufferAdapterTester::assertDataOut(const Fw::Buffer& buffer, const ComCfg::FrameContext& context) {
    ASSERT_from_dataOut_SIZE(1);
    this->assertSameBuffer(buffer, this->fromPortHistory_dataOut->at(0).data);
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).context, context);
    this->assertNoOtherOutput(OutputPort::DATA_OUT);
}

void ComDataBufferAdapterTester::assertBufferInReturn(const Fw::Buffer& buffer) {
    ASSERT_from_bufferInReturn_SIZE(1);
    this->assertSameBuffer(buffer, this->fromPortHistory_bufferInReturn->at(0).fwBuffer);
    this->assertNoOtherOutput(OutputPort::BUFFER_IN_RETURN);
}

void ComDataBufferAdapterTester::assertBufferOut(const Fw::Buffer& buffer) {
    ASSERT_from_bufferOut_SIZE(1);
    this->assertSameBuffer(buffer, this->fromPortHistory_bufferOut->at(0).fwBuffer);
    this->assertNoOtherOutput(OutputPort::BUFFER_OUT);
}

void ComDataBufferAdapterTester::assertDataReturnOut(const Fw::Buffer& buffer) {
    ASSERT_from_dataReturnOut_SIZE(1);
    this->assertSameBuffer(buffer, this->fromPortHistory_dataReturnOut->at(0).data);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).context, ComCfg::FrameContext());
    this->assertNoOtherOutput(OutputPort::DATA_RETURN_OUT);
}

void ComDataBufferAdapterTester::assertNoOtherOutput(OutputPort except) {
    if (except != OutputPort::DATA_OUT) {
        ASSERT_from_dataOut_SIZE(0);
    }
    if (except != OutputPort::BUFFER_IN_RETURN) {
        ASSERT_from_bufferInReturn_SIZE(0);
    }
    if (except != OutputPort::BUFFER_OUT) {
        ASSERT_from_bufferOut_SIZE(0);
    }
    if (except != OutputPort::DATA_RETURN_OUT) {
        ASSERT_from_dataReturnOut_SIZE(0);
    }
}

void ComDataBufferAdapterTester::assertSameBuffer(const Fw::Buffer& expected, const Fw::Buffer& actual) {
    ASSERT_EQ(expected.getData(), actual.getData());
    ASSERT_EQ(expected.getSize(), actual.getSize());
    ASSERT_EQ(expected.getContext(), actual.getContext());
}

// ----------------------------------------------------------------------
// Helper functions: random data
// ----------------------------------------------------------------------

Fw::Buffer ComDataBufferAdapterTester::randomBuffer() {
    const FwSizeType size = STest::Pick::lowerUpper(1, static_cast<U32>(sizeof(this->m_data)));
    for (FwSizeType i = 0; i < size; i++) {
        this->m_data[i] = static_cast<U8>(STest::Pick::any());
    }
    return Fw::Buffer(this->m_data, size, STest::Pick::any());
}

ComCfg::FrameContext ComDataBufferAdapterTester::randomContext() {
    ComCfg::FrameContext context;
    context.set_apid(static_cast<ComCfg::Apid::T>(
        STest::Pick::lowerUpper(ComCfg::Apid::FW_PACKET_COMMAND, ComCfg::Apid::FW_PACKET_PARAM)));
    context.set_vcId(static_cast<U8>(STest::Pick::any()));
    context.set_sequenceCount(static_cast<U16>(STest::Pick::any()));
    context.set_sendNow(STest::Pick::any() % 2 == 0);
    return context;
}

}  // namespace Svc
