// ======================================================================
// \title  SppZonePackerTester.cpp
// \author devin
// \brief  cpp file for SppZonePacker component test harness implementation class
// ======================================================================

#include "SppZonePackerTester.hpp"
#include "Svc/Ccsds/Types/SpacePacketHeaderSerializableAc.hpp"

namespace Svc {

namespace Ccsds {

// Out-of-line definitions for odr-used constants (C++14)
const FwSizeType SppZonePackerTester::TEST_ZONE_SIZE;
const FwSizeType SppZonePackerTester::TEST_HEADROOM;
const FwSizeType SppZonePackerTester::TEST_TRAILER;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

SppZonePackerTester ::SppZonePackerTester()
    : SppZonePackerGTestBase("SppZonePackerTester", SppZonePackerTester::MAX_HISTORY_SIZE), component("SppZonePacker") {
    this->initComponents();
    this->connectPorts();
    this->component.configure(TEST_ZONE_SIZE, TEST_HEADROOM, TEST_TRAILER, 1);
}

SppZonePackerTester ::~SppZonePackerTester() {}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void SppZonePackerTester ::sendPacket(Fw::Buffer& packet, bool sendNow) {
    for (FwSizeType i = 0; i < packet.getSize(); i++) {
        packet.getData()[i] = static_cast<U8>(i);
    }
    ComCfg::FrameContext context;
    context.set_sendNow(sendNow);
    this->invoke_to_dataIn(0, packet, context);
}

Fw::Buffer SppZonePackerTester ::getZone(FwIndexType historyIndex) {
    return this->fromPortHistory_dataOut->at(static_cast<U32>(historyIndex)).data;
}

ComCfg::FrameContext SppZonePackerTester ::getZoneContext(FwIndexType historyIndex) {
    return this->fromPortHistory_dataOut->at(static_cast<U32>(historyIndex)).context;
}

void SppZonePackerTester ::returnZone(FwIndexType historyIndex) {
    Fw::Buffer zone = this->getZone(historyIndex);
    ComCfg::FrameContext context = this->getZoneContext(historyIndex);
    this->invoke_to_dataReturnIn(0, zone, context);
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void SppZonePackerTester ::testMultiPacketsOneZone() {
    U8 data[3][20];
    for (U32 p = 0; p < 3; p++) {
        Fw::Buffer packet(data[p], sizeof(data[p]));
        this->sendPacket(packet);
        // Each packet fits with room to spare: returned upstream and credited
        ASSERT_from_dataReturnOut_SIZE(p + 1);
        ASSERT_from_comStatusOut_SIZE(p + 1);
        ASSERT_EQ(this->fromPortHistory_comStatusOut->at(p).condition, Fw::Success::SUCCESS);
        ASSERT_from_dataOut_SIZE(0);
    }

    // Flush the partial zone
    this->invoke_to_run(0, 0);
    ASSERT_from_dataOut_SIZE(1);
    Fw::Buffer zone = this->getZone(0);
    ComCfg::FrameContext context = this->getZoneContext(0);
    ASSERT_EQ(zone.getSize(), TEST_ZONE_SIZE);
    ASSERT_EQ(context.get_firstHeaderPointer(), 0);  // first packet starts at offset 0
    ASSERT_TRUE(context.get_zeroCopyFrame());

    // Packet bytes are packed back to back
    for (U32 p = 0; p < 3; p++) {
        for (U32 i = 0; i < 20; i++) {
            ASSERT_EQ(zone.getData()[(p * 20) + i], static_cast<U8>(i));
        }
    }
    // Idle fill after the packed packets: check idle data pattern past the idle header
    for (FwSizeType i = 60 + SpacePacketHeader::SERIALIZED_SIZE; i < TEST_ZONE_SIZE; i++) {
        ASSERT_EQ(zone.getData()[i], SppZonePacker::SPP_IDLE_DATA_PATTERN);
    }
    // The zone window preserves headroom and trailer reserve in the backing allocation
    ASSERT_EQ(zone.getOffset(), TEST_HEADROOM);
    ASSERT_GE(zone.getCapacity(), TEST_HEADROOM + TEST_ZONE_SIZE + TEST_TRAILER);
    ASSERT_EVENTS_ZoneFlushed_SIZE(1);
}

void SppZonePackerTester ::testPacketSpanning() {
    // A packet spanning three zones
    U8 data[250];
    Fw::Buffer packet(data, sizeof(data));
    this->sendPacket(packet);

    // Zone 1 sent immediately, packet held (not returned), no credit
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_from_dataReturnOut_SIZE(0);
    ASSERT_from_comStatusOut_SIZE(0);
    ASSERT_EQ(this->getZoneContext(0).get_firstHeaderPointer(), 0);
    for (U32 i = 0; i < 100; i++) {
        ASSERT_EQ(this->getZone(0).getData()[i], static_cast<U8>(i));
    }

    // Return zone 1: zone 2 (pure continuation) is sent
    this->returnZone(0);
    ASSERT_from_dataOut_SIZE(2);
    ASSERT_EQ(this->getZoneContext(1).get_firstHeaderPointer(),
              static_cast<U16>(ComCfg::FhpValues::FHP_NO_PACKET_START));
    ASSERT_from_dataReturnOut_SIZE(0);
    for (U32 i = 0; i < 100; i++) {
        ASSERT_EQ(this->getZone(1).getData()[i], static_cast<U8>(100 + i));
    }

    // Return zone 2: the 50-byte tail is packed into zone 3, packet returned and credited
    this->returnZone(1);
    ASSERT_from_dataOut_SIZE(2);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_comStatusOut->at(0).condition, Fw::Success::SUCCESS);

    // Downstream SUCCESS statuses for the two self-credited zones are absorbed
    Fw::Success success = Fw::Success::SUCCESS;
    this->invoke_to_comStatusIn(0, success);
    this->invoke_to_comStatusIn(0, success);
    ASSERT_from_comStatusOut_SIZE(1);
    // A third SUCCESS is a genuine recovery/initial status: passed through
    this->invoke_to_comStatusIn(0, success);
    ASSERT_from_comStatusOut_SIZE(2);
}

void SppZonePackerTester ::testContinuationTailThenFresh() {
    // 150-byte packet: zone 1 full, 50-byte tail into zone 2
    U8 data[150];
    Fw::Buffer packet(data, sizeof(data));
    this->sendPacket(packet);
    ASSERT_from_dataOut_SIZE(1);
    this->returnZone(0);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_from_comStatusOut_SIZE(1);

    // Absorb the downstream SUCCESS for zone 1
    Fw::Success success = Fw::Success::SUCCESS;
    this->invoke_to_comStatusIn(0, success);
    ASSERT_from_comStatusOut_SIZE(1);

    // A fresh packet follows the tail: FHP of zone 2 is the tail length
    U8 data2[10];
    Fw::Buffer packet2(data2, sizeof(data2));
    this->sendPacket(packet2);
    this->invoke_to_run(0, 0);
    ASSERT_from_dataOut_SIZE(2);
    ASSERT_EQ(this->getZoneContext(1).get_firstHeaderPointer(), 50);
}

void SppZonePackerTester ::testSendNowFlush() {
    U8 data[40];
    Fw::Buffer packet(data, sizeof(data));
    this->sendPacket(packet, true);

    // Zone idle-filled and sent immediately
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_EQ(this->getZone(0).getSize(), TEST_ZONE_SIZE);
    ASSERT_EQ(this->getZoneContext(0).get_firstHeaderPointer(), 0);
    ASSERT_from_dataReturnOut_SIZE(1);
    // Credit released on zone return
    ASSERT_from_comStatusOut_SIZE(0);
    this->returnZone(0);
    ASSERT_from_comStatusOut_SIZE(1);
}

void SppZonePackerTester ::testSchedulerFlush() {
    // Nothing pending: run is a no-op
    this->invoke_to_run(0, 0);
    ASSERT_from_dataOut_SIZE(0);

    U8 data[30];
    Fw::Buffer packet(data, sizeof(data));
    this->sendPacket(packet);
    ASSERT_from_dataOut_SIZE(0);

    this->invoke_to_run(0, 0);
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_EQ(this->getZone(0).getSize(), TEST_ZONE_SIZE);
    ASSERT_EVENTS_ZoneFlushed_SIZE(1);
    ASSERT_EVENTS_ZoneFlushed(0, TEST_ZONE_SIZE - 30);
}

void SppZonePackerTester ::testIdleStriping() {
    // Leave fewer bytes in the zone than a minimum Space Packet: the idle packet
    // is striped across the zone boundary
    U8 data[TEST_ZONE_SIZE - 3];
    Fw::Buffer packet(data, sizeof(data));
    this->sendPacket(packet, true);

    // Zone 1 sent with the first 3 bytes of the idle packet at its end
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_from_dataReturnOut_SIZE(1);

    // Return zone 1: the idle tail lands in zone 2, which is then idle-filled and sent
    this->returnZone(0);
    ASSERT_from_dataOut_SIZE(2);
    // Zone 2 holds only idle data (idle tail plus idle fill): idle-data-only sentinel
    ASSERT_EQ(this->getZoneContext(1).get_firstHeaderPointer(),
              static_cast<U16>(ComCfg::FhpValues::FHP_IDLE_DATA_ONLY));
}

void SppZonePackerTester ::testExactFill() {
    // A packet that exactly fills the zone
    U8 data[TEST_ZONE_SIZE];
    Fw::Buffer packet(data, sizeof(data));
    this->sendPacket(packet);

    // Zone sent, packet returned, but no credit until the zone buffer returns
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_EQ(this->getZoneContext(0).get_firstHeaderPointer(), 0);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_from_comStatusOut_SIZE(0);

    this->returnZone(0);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_comStatusOut->at(0).condition, Fw::Success::SUCCESS);

    // Downstream SUCCESS for the zone is absorbed
    Fw::Success success = Fw::Success::SUCCESS;
    this->invoke_to_comStatusIn(0, success);
    ASSERT_from_comStatusOut_SIZE(1);
}

void SppZonePackerTester ::testComStatusPassthrough() {
    // Initial SUCCESS with no credit owed: passed through to prime ComQueue
    Fw::Success success = Fw::Success::SUCCESS;
    this->invoke_to_comStatusIn(0, success);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_comStatusOut->at(0).condition, Fw::Success::SUCCESS);

    // FAILURE always passes through
    Fw::Success failure = Fw::Success::FAILURE;
    this->invoke_to_comStatusIn(0, failure);
    ASSERT_from_comStatusOut_SIZE(2);
    ASSERT_EQ(this->fromPortHistory_comStatusOut->at(1).condition, Fw::Success::FAILURE);
}

void SppZonePackerTester ::testFailureRecovery() {
    // Two zones in flight (three-zone span)
    U8 data[250];
    Fw::Buffer packet(data, sizeof(data));
    this->sendPacket(packet);
    this->returnZone(0);
    ASSERT_from_dataOut_SIZE(2);
    ASSERT_EQ(this->component.m_vcs[0].creditOwed, 2);

    // Downstream FAILURE: passes through and voids the pending credits
    Fw::Success failure = Fw::Success::FAILURE;
    this->invoke_to_comStatusIn(0, failure);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_comStatusOut->at(0).condition, Fw::Success::FAILURE);
    ASSERT_EQ(this->component.m_vcs[0].creditOwed, 0);

    // Recovery SUCCESS passes through
    Fw::Success success = Fw::Success::SUCCESS;
    this->invoke_to_comStatusIn(0, success);
    ASSERT_from_comStatusOut_SIZE(2);
    ASSERT_EQ(this->fromPortHistory_comStatusOut->at(1).condition, Fw::Success::SUCCESS);
}

void SppZonePackerTester ::testFlushWhileInFlight() {
    // Fill and send a zone, leaving a partial second zone
    U8 data[130];
    Fw::Buffer packet(data, sizeof(data));
    this->sendPacket(packet);
    ASSERT_from_dataOut_SIZE(1);
    this->returnZone(0);
    ASSERT_from_dataOut_SIZE(1);  // 30-byte tail packed into zone 2, not sent
    ASSERT_from_comStatusOut_SIZE(1);

    // Flush while the credit for zone 1 is still outstanding: deferred
    this->invoke_to_run(0, 0);
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_TRUE(this->component.m_vcs[0].flushPending);

    // The downstream SUCCESS both absorbs the credit and triggers the deferred flush
    Fw::Success success = Fw::Success::SUCCESS;
    this->invoke_to_comStatusIn(0, success);
    ASSERT_from_dataOut_SIZE(2);
    ASSERT_FALSE(this->component.m_vcs[0].flushPending);
    ASSERT_EQ(this->getZoneContext(1).get_firstHeaderPointer(), 30);
}

}  // namespace Ccsds

}  // namespace Svc
