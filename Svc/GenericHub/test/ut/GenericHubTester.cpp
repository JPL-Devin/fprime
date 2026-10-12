// ======================================================================
// \title  GenericHub.hpp
// \author mstarch
// \brief  cpp file for GenericHub test harness implementation class
//
// \copyright
// Copyright 2009-2015, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#include "GenericHubTester.hpp"
#include <STest/Pick/Pick.hpp>
#include "Fw/Com/ComPacket.hpp"

#define INSTANCE 0
#define MAX_HISTORY_SIZE 10000

namespace Svc {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

GenericHubTester ::GenericHubTester()
    : GenericHubGTestBase("Tester", MAX_HISTORY_SIZE),
      componentIn("GenericHubIn"),
      componentOut("GenericHubOut"),
      m_buffer(m_data_store, DATA_SIZE),
      m_comm_in(0),
      m_buffer_in(0),
      m_comm_out(0),
      m_buffer_out(0),
      m_current_port(0),
      m_cmdDispOutPort(0),
      m_cmdRespOutPort(0),
      m_allocation_mode(ALLOCATE_OK),
      m_allocation_failures(0) {
    this->initComponents();
    this->connectPorts();
}

GenericHubTester ::~GenericHubTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void GenericHubTester ::test_in_out() {
    U32 max =
        std::min(this->componentIn.getNum_serialIn_InputPorts(), this->componentOut.getNum_serialOut_OutputPorts());
    for (U32 i = 0; i < max; i++) {
        send_random_comm(i);
        ASSERT_from_fromBufferDriverReturn_SIZE(1);
        fromPortHistory_fromBufferDriverReturn->clear();
    }
}

void GenericHubTester ::test_buffer_io() {
    U32 max =
        std::min(this->componentIn.getNum_bufferIn_InputPorts(), this->componentOut.getNum_bufferOut_OutputPorts());
    for (U32 i = 0; i < max; i++) {
        send_random_buffer(i);
        ASSERT_from_fromBufferDriverReturn_SIZE(1);
        fromPortHistory_fromBufferDriverReturn->clear();
    }
}

void GenericHubTester ::test_random_io() {
    for (U32 i = 0; i < 10000; i++) {
        U32 choice = STest::Pick::lowerUpper(0, 1);
        if (choice) {
            U32 port = STest::Pick::lowerUpper(0, std::min(this->componentIn.getNum_serialIn_InputPorts(),
                                                           this->componentOut.getNum_serialOut_OutputPorts()) -
                                                      1);
            send_random_comm(port);
        } else {
            U32 port = STest::Pick::lowerUpper(0, std::min(this->componentIn.getNum_bufferIn_InputPorts(),
                                                           this->componentOut.getNum_bufferOut_OutputPorts()) -
                                                      1);
            send_random_buffer(port);
        }
        ASSERT_from_fromBufferDriverReturn_SIZE(1);
        fromPortHistory_fromBufferDriverReturn->clear();
    }
}

void GenericHubTester ::random_fill(Fw::SerialBufferBase& buffer, U32 max_size) {
    fill(buffer, STest::Pick::lowerUpper(0, max_size));
}

void GenericHubTester ::fill(Fw::SerialBufferBase& buffer, U32 size) {
    buffer.resetSer();
    for (U32 i = 0; i < size; i++) {
        ASSERT_EQ(Fw::FW_SERIALIZE_OK, buffer.serializeFrom(static_cast<U8>(STest::Pick::any())));
    }
}

void GenericHubTester ::test_telemetry() {
    test_telemetry_size(STest::Pick::lowerUpper(0, FW_TLM_BUFFER_MAX_SIZE));
}

void GenericHubTester ::test_telemetry_size(U32 size) {
    Fw::TlmBuffer buffer;
    clearFromPortHistory();
    fill(buffer, size);

    Fw::Time time(100, 200);
    invoke_to_tlmIn(0, 123, time, buffer);

    ASSERT_from_fromBufferDriverReturn_SIZE(1);  // **must** return buffer
    ASSERT_from_tlmOut_SIZE(1);
    ASSERT_from_tlmOut(0, 123, time, buffer);
    clearFromPortHistory();
}

void GenericHubTester ::test_events() {
    test_events_size(STest::Pick::lowerUpper(0, FW_LOG_BUFFER_MAX_SIZE));
}

void GenericHubTester ::test_events_size(U32 size) {
    Fw::LogSeverity severity = Fw::LogSeverity::WARNING_HI;
    Fw::LogBuffer buffer;
    clearFromPortHistory();
    fill(buffer, size);

    Fw::Time time(100, 200);
    invoke_to_eventIn(0, 123, time, severity, buffer);

    ASSERT_from_fromBufferDriverReturn_SIZE(1);  // **must** deallocate buffer
    ASSERT_from_eventOut_SIZE(1);
    ASSERT_from_eventOut(0, 123, time, severity, buffer);
    clearFromPortHistory();
}
// Helpers

void GenericHubTester ::send_random_comm(U32 port) {
    random_fill(m_comm, FW_COM_BUFFER_MAX_SIZE);
    m_current_port = port;
    invoke_to_serialIn(m_current_port, m_comm);
    // Ensure that the data out was called, and that the serialOut unwrapped properly
    ASSERT_from_toBufferDriver_SIZE(m_comm_in + m_buffer_out + 1);
    ASSERT_EQ(m_comm_in + 1, m_comm_out);
    m_comm_in++;
}

void GenericHubTester ::send_random_buffer(U32 port) {
    U32 max_random_size = STest::Pick::lowerUpper(0, DATA_SIZE - (sizeof(U32) + sizeof(U32) + sizeof(FwBuffSizeType)));
    m_buffer.set(m_data_store, sizeof(m_data_store));
    ASSERT_GE(m_buffer.getSize(), max_random_size);
    auto serializer = m_buffer.getSerializer();
    random_fill(serializer, max_random_size);
    m_buffer.setSize(max_random_size);
    m_current_port = port;
    invoke_to_bufferIn(m_current_port, m_buffer);
    ASSERT_from_bufferInReturn_SIZE(1);
    ASSERT_from_bufferInReturn(0, m_buffer);
    fromPortHistory_bufferInReturn->clear();
    // Ensure that the data out was called, and that the serialOut unwrapped properly
    ASSERT_from_toBufferDriver_SIZE(m_buffer_in + m_comm_out + 1);
    ASSERT_EQ(m_buffer_in + 1, m_buffer_out);
    m_buffer_in++;
}

void GenericHubTester ::test_command_dispatch() {
    Fw::ComBuffer buffer;
    clearFromPortHistory();
    random_fill(buffer, FW_TLM_BUFFER_MAX_SIZE);

    invoke_to_cmdDispIn(0, buffer, 279);

    // **must** return buffer
    ASSERT_from_fromBufferDriverReturn_SIZE(1);
    ASSERT_from_cmdDispOut_SIZE(1);
    ASSERT_from_cmdDispOut(0, buffer, 279);
    clearFromPortHistory();

    // A maximum-size command payload must round-trip without overflow
    buffer.resetSer();
    for (U32 i = 0; i < FW_COM_BUFFER_MAX_SIZE; i++) {
        ASSERT_EQ(Fw::FW_SERIALIZE_OK, buffer.serializeFrom(static_cast<U8>(STest::Pick::any())));
    }
    invoke_to_cmdDispIn(0, buffer, 280);

    ASSERT_from_fromBufferDriverReturn_SIZE(1);
    ASSERT_from_cmdDispOut_SIZE(1);
    ASSERT_from_cmdDispOut(0, buffer, 280);
    clearFromPortHistory();
}

void GenericHubTester ::test_command_response() {
    FwOpcodeType opCode = 0x3A56BF8C;
    U32 cmdSeq = 825;
    Fw::CmdResponse response = Fw::CmdResponse::VALIDATION_ERROR;

    invoke_to_cmdRespIn(0, opCode, cmdSeq, response);

    ASSERT_from_cmdRespOut_SIZE(1);
    ASSERT_from_cmdRespOut(0, 0x3A56BF8C, 825, Fw::CmdResponse::VALIDATION_ERROR);
    clearFromPortHistory();
}

void GenericHubTester ::test_commands() {
    this->test_command_dispatch();

    this->test_command_response();
}

void GenericHubTester ::test_commands_nonzero_port() {
    const FwIndexType port = 1;
    Fw::ComBuffer buffer;
    clearFromPortHistory();
    random_fill(buffer, FW_TLM_BUFFER_MAX_SIZE);

    invoke_to_cmdDispIn(port, buffer, 279);

    // **must** return buffer and forward on the same nonzero port index
    ASSERT_from_fromBufferDriverReturn_SIZE(1);
    ASSERT_from_cmdDispOut_SIZE(1);
    ASSERT_from_cmdDispOut(0, buffer, 279);
    ASSERT_EQ(m_cmdDispOutPort, port);
    clearFromPortHistory();

    invoke_to_cmdRespIn(port, 0x3A56BF8C, 825, Fw::CmdResponse::VALIDATION_ERROR);

    ASSERT_from_cmdRespOut_SIZE(1);
    ASSERT_from_cmdRespOut(0, 0x3A56BF8C, 825, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EQ(m_cmdRespOutPort, port);
    clearFromPortHistory();
}

void GenericHubTester ::send_from_driver_packet(U32 type,
                                                U32 port,
                                                FwBuffSizeType declaredSize,
                                                const U8* payload,
                                                FwBuffSizeType payloadSize) {
    Fw::SerializeStatus status = Fw::FW_SERIALIZE_OK;
    Fw::ExternalSerializeBuffer serializer(m_data_for_allocation, sizeof(m_data_for_allocation));
    serializer.resetSer();

    status = serializer.serializeFrom(type);
    ASSERT_EQ(status, Fw::FW_SERIALIZE_OK);
    status = serializer.serializeFrom(port);
    ASSERT_EQ(status, Fw::FW_SERIALIZE_OK);
    status = serializer.serializeFrom(declaredSize);
    ASSERT_EQ(status, Fw::FW_SERIALIZE_OK);

    if (payloadSize > 0) {
        ASSERT_NE(payload, nullptr);
        status = serializer.serializeFrom(payload, payloadSize, Fw::Serialization::OMIT_LENGTH);
        ASSERT_EQ(status, Fw::FW_SERIALIZE_OK);
    }

    Fw::Buffer buffer(m_data_for_allocation, serializer.getSize());
    this->invoke_to_fromBufferDriver(0, buffer);
}

void GenericHubTester ::assert_no_outputs_sent() {
    ASSERT_EQ(m_comm_out, 0U);
    ASSERT_from_bufferOut_SIZE(0);
    ASSERT_from_eventOut_SIZE(0);
    ASSERT_from_tlmOut_SIZE(0);
    ASSERT_from_cmdDispOut_SIZE(0);
    ASSERT_from_cmdRespOut_SIZE(0);
}

void GenericHubTester ::test_invalid_deserialization_paths() {
    // Header too short to deserialize
    clearFromPortHistory();
    Fw::Buffer shortBuffer(m_data_for_allocation, sizeof(U32) + sizeof(U32) + sizeof(FwBuffSizeType) - 1);
    this->invoke_to_fromBufferDriver(0, shortBuffer);
    ASSERT_from_fromBufferDriverReturn_SIZE(1);
    this->assert_no_outputs_sent();

    // Invalid type should be dropped
    clearFromPortHistory();
    this->send_from_driver_packet(static_cast<U32>(GenericHub::HUB_TYPE_MAX), 0, 0, nullptr, 0);
    ASSERT_from_fromBufferDriverReturn_SIZE(1);
    this->assert_no_outputs_sent();

    // Declared payload size mismatch should be dropped
    clearFromPortHistory();
    this->send_from_driver_packet(static_cast<U32>(GenericHub::HUB_TYPE_PORT), 0, 1, nullptr, 0);
    ASSERT_from_fromBufferDriverReturn_SIZE(1);
    this->assert_no_outputs_sent();

    // Invalid serialOut destination port should not invoke output
    clearFromPortHistory();
    this->send_from_driver_packet(static_cast<U32>(GenericHub::HUB_TYPE_PORT),
                                  this->componentOut.getNum_serialOut_OutputPorts(), 0, nullptr, 0);
    ASSERT_from_fromBufferDriverReturn_SIZE(1);
    this->assert_no_outputs_sent();

    // Invalid bufferOut destination port should return the buffer
    clearFromPortHistory();
    this->send_from_driver_packet(static_cast<U32>(GenericHub::HUB_TYPE_BUFFER),
                                  this->componentOut.getNum_bufferOut_OutputPorts(), 0, nullptr, 0);
    ASSERT_from_fromBufferDriverReturn_SIZE(1);
    this->assert_no_outputs_sent();

    // Command dispatch path: raw payload too small to include context
    clearFromPortHistory();
    this->send_from_driver_packet(static_cast<U32>(GenericHub::HUB_TYPE_CMD_DISP), 0, 0, nullptr, 0);
    ASSERT_from_fromBufferDriverReturn_SIZE(1);
    this->assert_no_outputs_sent();

    // Command dispatch path: raw payload larger than ComBuffer supports
    clearFromPortHistory();
    const FwBuffSizeType tooLargeCmdSize =
        static_cast<FwBuffSizeType>(Fw::ComBuffer::SERIALIZED_SIZE + sizeof(U32) + 1);
    this->send_from_driver_packet(static_cast<U32>(GenericHub::HUB_TYPE_CMD_DISP), 0, tooLargeCmdSize, m_data_store,
                                  tooLargeCmdSize);
    ASSERT_from_fromBufferDriverReturn_SIZE(1);
    this->assert_no_outputs_sent();

    clearFromPortHistory();
}

void GenericHubTester ::assert_dropped(GenericHub::HubType type, FwIndexType port, FwSizeType payloadSize) {
    const FwSizeType needed = payloadSize + sizeof(U32) + sizeof(U32) + sizeof(FwBuffSizeType);
    ASSERT_from_toBufferDriver_SIZE(0);
    // Only a non-null (undersized) buffer is handed back to the allocator; telemetry is only written from schedIn
    if (m_allocation_mode == ALLOCATE_UNDERSIZED) {
        ASSERT_from_deallocate_SIZE(1);
        ASSERT_EQ(this->fromPortHistory_deallocate->at(0).fwBuffer.getData(), m_data_for_allocation);
        ASSERT_EQ(this->fromPortHistory_deallocate->at(0).fwBuffer.getSize(), needed - 1);
    } else {
        ASSERT_from_deallocate_SIZE(0);
    }
    ASSERT_TLM_NumAllocationFailures_SIZE(0);
    // The event is throttled after EVENTID_BUFFERALLOCATIONFAILED_THROTTLE drops; the counter is not
    if (m_allocation_failures < GenericHubComponentBase::EVENTID_BUFFERALLOCATIONFAILED_THROTTLE) {
        ASSERT_EVENTS_BufferAllocationFailed_SIZE(1);
        ASSERT_EVENTS_BufferAllocationFailed(0, static_cast<U32>(type), port, needed);
    } else {
        ASSERT_EVENTS_BufferAllocationFailed_SIZE(0);
    }
    m_allocation_failures++;
    this->clearHistory();
}

void GenericHubTester ::test_allocation_failure() {
    this->clearHistory();
    m_allocation_mode = ALLOCATE_EMPTY;

    // Serial port call is dropped
    Fw::ComBuffer comm;
    random_fill(comm, FW_COM_BUFFER_MAX_SIZE);
    invoke_to_serialIn(1, comm);
    assert_dropped(GenericHub::HUB_TYPE_PORT, 1, comm.getSize());

    // Buffer is dropped but still returned to its owner
    m_buffer.set(m_data_store, STest::Pick::lowerUpper(1, FW_COM_BUFFER_MAX_SIZE));
    invoke_to_bufferIn(2, m_buffer);
    ASSERT_from_bufferInReturn_SIZE(1);
    ASSERT_from_bufferInReturn(0, m_buffer);
    assert_dropped(GenericHub::HUB_TYPE_BUFFER, 2, m_buffer.getSize());

    // Event is dropped
    Fw::LogBuffer logArgs;
    Fw::Time time(100, 200);
    fill(logArgs, STest::Pick::lowerUpper(0, FW_LOG_BUFFER_MAX_SIZE));
    invoke_to_eventIn(0, 123, time, Fw::LogSeverity::WARNING_HI, logArgs);
    assert_dropped(GenericHub::HUB_TYPE_EVENT, 0,
                   sizeof(FwEventIdType) + Fw::Time::SERIALIZED_SIZE + Fw::LogSeverity::SERIALIZED_SIZE +
                       sizeof(FwSizeStoreType) + logArgs.getSize());

    // Telemetry is dropped
    Fw::TlmBuffer tlmVal;
    fill(tlmVal, STest::Pick::lowerUpper(0, FW_TLM_BUFFER_MAX_SIZE));
    invoke_to_tlmIn(0, 123, time, tlmVal);
    assert_dropped(GenericHub::HUB_TYPE_CHANNEL, 0,
                   sizeof(FwChanIdType) + Fw::Time::SERIALIZED_SIZE + sizeof(FwSizeStoreType) + tlmVal.getSize());

    // Command response is dropped
    invoke_to_cmdRespIn(1, 0x3A56BF8C, 825, Fw::CmdResponse::OK);
    assert_dropped(GenericHub::HUB_TYPE_CMD_RESP, 1,
                   sizeof(FwOpcodeType) + sizeof(U32) + Fw::CmdResponse::SERIALIZED_SIZE);

    // An undersized buffer is dropped and returned to the allocator; the event is now throttled
    m_allocation_mode = ALLOCATE_UNDERSIZED;
    invoke_to_serialIn(0, comm);
    assert_dropped(GenericHub::HUB_TYPE_PORT, 0, comm.getSize());

    // Counter is reported on schedIn
    invoke_to_schedIn(0, 0);
    ASSERT_TLM_NumAllocationFailures_SIZE(1);
    ASSERT_TLM_NumAllocationFailures(0, m_allocation_failures);
    this->clearHistory();

    // Counter keeps counting while the event is throttled
    m_allocation_mode = ALLOCATE_EMPTY;
    for (U32 i = 0; i < 10; i++) {
        invoke_to_serialIn(0, comm);
        assert_dropped(GenericHub::HUB_TYPE_PORT, 0, comm.getSize());
    }
    invoke_to_schedIn(0, 0);
    ASSERT_TLM_NumAllocationFailures(0, m_allocation_failures);
    this->clearHistory();

    // Hub keeps working once allocation succeeds again
    m_allocation_mode = ALLOCATE_OK;
    send_random_comm(0);
    ASSERT_EVENTS_BufferAllocationFailed_SIZE(0);
}

void GenericHubTester ::test_command_dispatch_allocation_failure() {
    const FwIndexType port = 1;
    const FwOpcodeType opCode = 0x1234;
    const U32 context = 77;
    Fw::ComBuffer buffer;
    this->clearHistory();
    // Build a command packet: descriptor, opcode, then random argument bytes
    ASSERT_EQ(Fw::FW_SERIALIZE_OK,
              buffer.serializeFrom(static_cast<FwPacketDescriptorType>(Fw::ComPacketType::FW_PACKET_COMMAND)));
    ASSERT_EQ(Fw::FW_SERIALIZE_OK, buffer.serializeFrom(opCode));
    const FwSizeType argBytes = STest::Pick::lowerUpper(0, FW_CMD_ARG_BUFFER_MAX_SIZE);
    for (FwSizeType i = 0; i < argBytes; i++) {
        ASSERT_EQ(Fw::FW_SERIALIZE_OK, buffer.serializeFrom(static_cast<U8>(STest::Pick::any())));
    }

    m_allocation_mode = ALLOCATE_EMPTY;
    invoke_to_cmdDispIn(port, buffer, context);

    // Dropped command is failed back to the caller on the same port index
    ASSERT_from_cmdDispOut_SIZE(0);
    ASSERT_from_cmdRespOut_SIZE(1);
    ASSERT_from_cmdRespOut(0, opCode, context, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EQ(m_cmdRespOutPort, port);
    assert_dropped(GenericHub::HUB_TYPE_CMD_DISP, port, buffer.getSize() + sizeof(U32));

    // A malformed (non-command) packet that is dropped is still failed back, with the reserved opcode 0
    buffer.resetSer();
    ASSERT_EQ(Fw::FW_SERIALIZE_OK,
              buffer.serializeFrom(static_cast<FwPacketDescriptorType>(Fw::ComPacketType::FW_PACKET_TELEM)));
    invoke_to_cmdDispIn(port, buffer, context);
    ASSERT_from_cmdDispOut_SIZE(0);
    ASSERT_from_cmdRespOut_SIZE(1);
    ASSERT_from_cmdRespOut(0, 0, context, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EQ(m_cmdRespOutPort, port);
    assert_dropped(GenericHub::HUB_TYPE_CMD_DISP, port, buffer.getSize() + sizeof(U32));
}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

void GenericHubTester ::from_eventOut_handler(const FwIndexType portNum,
                                              FwEventIdType id,
                                              Fw::Time& timeTag,
                                              const Fw::LogSeverity& severity,
                                              Fw::LogBuffer& args) {
    this->pushFromPortEntry_eventOut(id, timeTag, severity, args);
}

void GenericHubTester ::from_tlmOut_handler(const FwIndexType portNum,
                                            FwChanIdType id,
                                            Fw::Time& timeTag,
                                            Fw::TlmBuffer& val) {
    this->pushFromPortEntry_tlmOut(id, timeTag, val);
}

void GenericHubTester ::from_cmdDispOut_handler(const FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    this->m_cmdDispOutPort = portNum;
    this->pushFromPortEntry_cmdDispOut(data, context);
}

void GenericHubTester ::from_cmdRespOut_handler(const FwIndexType portNum,
                                                FwOpcodeType opCode,
                                                U32 cmdSeq,
                                                const Fw::CmdResponse& response) {
    this->m_cmdRespOutPort = portNum;
    this->pushFromPortEntry_cmdRespOut(opCode, cmdSeq, response);
}

void GenericHubTester ::from_toBufferDriver_handler(const FwIndexType portNum, Fw::Buffer& fwBuffer) {
    ASSERT_NE(fwBuffer.getData(), nullptr) << "Empty buffer to deallocate";
    ASSERT_GE(fwBuffer.getData(), m_data_for_allocation) << "Incorrect data pointer deallocated";
    ASSERT_LT(fwBuffer.getData(), m_data_for_allocation + sizeof(m_data_for_allocation))
        << "Incorrect data pointer deallocated";
    // Reuse m_allocate to pass into the otherside of the hub
    this->pushFromPortEntry_toBufferDriver(fwBuffer);
    invoke_to_fromBufferDriver(0, fwBuffer);
}

// ----------------------------------------------------------------------
// Handlers for serial from ports
// ----------------------------------------------------------------------

void GenericHubTester ::from_bufferOut_handler(const FwIndexType portNum, Fw::Buffer& fwBuffer) {
    m_buffer_out++;
    // Assert the buffer came through exactly on the right port
    ASSERT_EQ(portNum, m_current_port);
    ASSERT_EQ(fwBuffer.getSize(), m_buffer.getSize());
    for (U32 i = 0; i < fwBuffer.getSize(); i++) {
        U8 byte1 = reinterpret_cast<U8*>(fwBuffer.getData())[i];
        U8 byte2 = reinterpret_cast<U8*>(m_buffer.getData())[i];
        ASSERT_EQ(byte1, byte2);
    }

    this->invoke_to_bufferOutReturn(portNum, fwBuffer);
}

void GenericHubTester ::from_serialOut_handler(FwIndexType portNum,         /*!< The port number*/
                                               Fw::LinearBufferBase& Buffer /*!< The serialization buffer*/
) {
    m_comm_out++;
    // Assert the buffer came through exactly on the right port
    ASSERT_EQ(portNum, m_current_port);
    ASSERT_EQ(Buffer.getSize(), m_comm.getSize());
    for (U32 i = 0; i < Buffer.getSize(); i++) {
        ASSERT_EQ(Buffer.getBuffAddr()[i], m_comm.getBuffAddr()[i]);
    }
    ASSERT_from_bufferOut_SIZE(0);
}

Fw::Buffer GenericHubTester ::from_allocate_handler(const FwIndexType portNum, const FwSizeType size) {
    EXPECT_EQ(m_allocate.getData(), nullptr) << "Allocation buffer is still in use";
    EXPECT_LE(size, sizeof(m_data_for_allocation)) << "Allocation buffer size mismatch";
    switch (m_allocation_mode) {
        case ALLOCATE_EMPTY:
            return Fw::Buffer();
        case ALLOCATE_UNDERSIZED:
            m_allocate.set(m_data_for_allocation, size - 1);
            break;
        default:
            m_allocate.set(m_data_for_allocation, size);
            break;
    }
    return m_allocate;
}

void GenericHubTester ::from_deallocate_handler(const FwIndexType portNum, Fw::Buffer& fwBuffer) {
    this->pushFromPortEntry_deallocate(fwBuffer);
    m_allocate.set(nullptr, 0);
}

void GenericHubTester ::from_bufferInReturn_handler(const FwIndexType portNum, Fw::Buffer& fwBuffer) {
    // Check buffer deallocations here
    ASSERT_EQ(fwBuffer.getData(), m_buffer.getData()) << "Ensure that the buffer was deallocated";
    ASSERT_EQ(fwBuffer.getSize(), m_buffer.getSize()) << "Ensure that the buffer was deallocated";
    this->pushFromPortEntry_bufferInReturn(fwBuffer);
}

void GenericHubTester ::from_fromBufferDriverReturn_handler(const FwIndexType portNum, Fw::Buffer& fwBuffer) {
    ASSERT_NE(fwBuffer.getData(), nullptr) << "Empty buffer to deallocate";
    ASSERT_GE(fwBuffer.getData(), m_data_for_allocation) << "Incorrect data pointer deallocated";
    ASSERT_LT(fwBuffer.getData(), m_data_for_allocation + sizeof(m_data_for_allocation))
        << "Incorrect data pointer deallocated";

    this->pushFromPortEntry_fromBufferDriverReturn(fwBuffer);
    invoke_to_toBufferDriverReturn(portNum, fwBuffer);
    m_allocate.set(nullptr, 0);
}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void GenericHubTester ::connectPorts() {
    // bufferIn
    U32 max =
        std::min(this->componentIn.getNum_bufferIn_InputPorts(), this->componentOut.getNum_bufferOut_OutputPorts());
    for (U32 i = 0; i < max; ++i) {
        this->connect_to_bufferIn(i, this->componentIn.get_bufferIn_InputPort(i));
    }

    // eventIn
    this->connect_to_eventIn(0, this->componentIn.get_eventIn_InputPort(0));

    // tlmIn
    this->connect_to_tlmIn(0, this->componentIn.get_tlmIn_InputPort(0));

    // cmdDispIn
    for (FwIndexType i = 0; i < this->componentIn.getNum_cmdDispIn_InputPorts(); i++) {
        this->connect_to_cmdDispIn(i, this->componentIn.get_cmdDispIn_InputPort(i));
    }

    // cmdRespIn
    for (FwIndexType i = 0; i < this->componentIn.getNum_cmdRespIn_InputPorts(); i++) {
        this->connect_to_cmdRespIn(i, this->componentIn.get_cmdRespIn_InputPort(i));
    }

    // fromBufferDriver
    this->connect_to_fromBufferDriver(0, this->componentOut.get_fromBufferDriver_InputPort(0));

    // bufferOut
    for (U32 i = 0; i < max; ++i) {
        this->componentOut.set_bufferOut_OutputPort(i, this->get_from_bufferOut(i));
    }

    // eventOut
    this->componentOut.set_eventOut_OutputPort(0, this->get_from_eventOut(0));

    // tlmOut
    this->componentOut.set_tlmOut_OutputPort(0, this->get_from_tlmOut(0));

    // cmdDispOut
    for (FwIndexType i = 0; i < this->componentOut.getNum_cmdDispOut_OutputPorts(); i++) {
        this->componentOut.set_cmdDispOut_OutputPort(i, this->get_from_cmdDispOut(i));
    }

    // cmdRespOut
    for (FwIndexType i = 0; i < this->componentOut.getNum_cmdRespOut_OutputPorts(); i++) {
        this->componentOut.set_cmdRespOut_OutputPort(i, this->get_from_cmdRespOut(i));
    }

    // toBufferDriver
    this->componentIn.set_toBufferDriver_OutputPort(0, this->get_from_toBufferDriver(0));

    // toBufferDriverReturn
    this->connect_to_toBufferDriverReturn(0, this->componentIn.get_toBufferDriverReturn_InputPort(0));

    // bufferOutReturn
    for (U32 i = 0; i < max; ++i) {
        this->connect_to_bufferOutReturn(i, this->componentOut.get_bufferOutReturn_InputPort(i));
    }

    // bufferAllocate
    this->componentIn.set_allocate_OutputPort(0, this->get_from_allocate(0));

    // buffer Return
    this->componentOut.set_fromBufferDriverReturn_OutputPort(0, this->get_from_fromBufferDriverReturn(0));

    // dataDeallocate
    this->componentIn.set_deallocate_OutputPort(0, this->get_from_deallocate(0));

    // schedIn, events, telemetry and time (sending side only)
    this->connect_to_schedIn(0, this->componentIn.get_schedIn_InputPort(0));
    this->componentIn.set_Log_OutputPort(0, this->get_from_Log(0));
    this->componentIn.set_LogText_OutputPort(0, this->get_from_LogText(0));
    this->componentIn.set_Time_OutputPort(0, this->get_from_Time(0));
    this->componentIn.set_Tlm_OutputPort(0, this->get_from_Tlm(0));
    // Sending side fails dropped commands back to their caller
    for (FwIndexType i = 0; i < GenericHubComponentBase::NUM_CMDRESPOUT_OUTPUT_PORTS; i++) {
        this->componentIn.set_cmdRespOut_OutputPort(i, this->get_from_cmdRespOut(i));
    }

    // bufferInReturn
    for (FwIndexType i = 0; i < GenericHubCfg::NumBufferInputPorts; i++) {
        this->componentIn.set_bufferInReturn_OutputPort(i, this->get_from_bufferInReturn(i));
    }

    // ----------------------------------------------------------------------
    // Connect serial output ports
    // ----------------------------------------------------------------------
    max = std::min(this->componentIn.getNum_serialIn_InputPorts(), this->componentOut.getNum_serialOut_OutputPorts());
    for (U32 i = 0; i < max; ++i) {
        this->componentOut.set_serialOut_OutputPort(i, this->get_from_serialOut(i));
    }

    // ----------------------------------------------------------------------
    // Connect serial input ports
    // ----------------------------------------------------------------------
    // serialIn
    for (U32 i = 0; i < max; ++i) {
        this->connect_to_serialIn(i, this->componentIn.get_serialIn_InputPort(i));
    }
}

void GenericHubTester ::initComponents() {
    this->init();
    this->componentIn.init(INSTANCE);
    this->componentOut.init(INSTANCE + 1);
}

}  // end namespace Svc
