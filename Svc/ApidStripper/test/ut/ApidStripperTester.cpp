// ======================================================================
// \title  ApidStripperTester.cpp
// \brief  cpp file for ApidStripper component test harness implementation class
// ======================================================================

#include "ApidStripperTester.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

ApidStripperTester ::ApidStripperTester()
    : ApidStripperGTestBase("ApidStripperTester", ApidStripperTester::MAX_HISTORY_SIZE), component("ApidStripper") {
    this->initComponents();
    this->connectPorts();
}

ApidStripperTester ::~ApidStripperTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void ApidStripperTester ::testNominalStrip() {
    //             | APID (FW_PACKET_FILE) | Payload    |
    U8 bufferData[5] = {0x00, 0x03, 0xAA, 0xBB, 0xCC};
    Fw::Buffer buffer(bufferData, sizeof(bufferData));

    this->invoke_to_dataIn(0, buffer);

    ASSERT_from_dataOut_SIZE(1);
    ASSERT_from_dataReturnOut_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
    Fw::Buffer outputBuffer = this->fromPortHistory_dataOut->at(0).fwBuffer;
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).apid, ComCfg::Apid::FW_PACKET_FILE);
    ASSERT_EQ(outputBuffer.getSize(), sizeof(bufferData) - sizeof(FwPacketDescriptorType));
    ASSERT_EQ(outputBuffer.getData(), bufferData + sizeof(FwPacketDescriptorType));
    ASSERT_EQ(outputBuffer.getData()[0], 0xAA);
}

void ApidStripperTester ::testOutOfRangeApid() {
    //             | Out-of-range APID | Payload |
    U8 bufferData[3] = {0x05, 0x7B, 0xAA};
    Fw::Buffer buffer(bufferData, sizeof(bufferData));

    this->invoke_to_dataIn(0, buffer);

    ASSERT_from_dataOut_SIZE(1);
    ASSERT_EVENTS_SIZE(0);
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).apid, ComCfg::Apid::INVALID_UNINITIALIZED);
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).fwBuffer.getSize(), 1);
}

void ApidStripperTester ::testBufferTooSmall() {
    U8 bufferData[1] = {0x00};
    Fw::Buffer buffer(bufferData, sizeof(bufferData));

    this->invoke_to_dataIn(0, buffer);

    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(1);  // buffer returned to sender
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_BufferTooSmall_SIZE(1);
}

void ApidStripperTester ::testDataReturn() {
    U8 bufferData[4] = {0};
    Fw::Buffer buffer(bufferData, sizeof(bufferData));
    this->invoke_to_dataReturnIn(0, buffer);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).fwBuffer.getData(), bufferData);
}

}  // namespace Svc
