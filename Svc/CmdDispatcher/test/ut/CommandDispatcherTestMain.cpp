/*
 * CommandDispatcherTester.cpp
 *
 *  Created on: Mar 18, 2015
 *      Author: tcanham
 */

#include <gtest/gtest.h>
#include <Fw/Obj/SimpleObjRegistry.hpp>
#include <Fw/Test/UnitTest.hpp>
#include <Svc/CmdDispatcher/CommandDispatcherImpl.hpp>
#include <Svc/CmdDispatcher/test/ut/CommandDispatcherTester.hpp>
#include <config/CommandDispatcherImplCfg.hpp>

#include <limits>

void connectPorts(Svc::CommandDispatcherImpl& impl, Svc::CommandDispatcherTester& tester) {
    // Fw::SimpleObjRegistry simpleReg;

    // command ports
    tester.connect_to_compCmdStat(0, impl.get_compCmdStat_InputPort(0));
    tester.connect_to_seqCmdBuff(0, impl.get_seqCmdBuff_InputPort(0));
    tester.connect_to_compCmdReg(0, impl.get_compCmdReg_InputPort(0));
    tester.connect_to_run(0, impl.get_run_InputPort(0));

    impl.set_compCmdSend_OutputPort(0, tester.get_from_compCmdSend(0));
    impl.set_seqCmdStatus_OutputPort(0, tester.get_from_seqCmdStatus(0));
    // local dispatcher command registration
    impl.set_CmdReg_OutputPort(0, impl.get_compCmdReg_InputPort(1));
    impl.set_CmdStatus_OutputPort(0, impl.get_compCmdStat_InputPort(0));

    impl.set_compCmdSend_OutputPort(1, impl.get_CmdDisp_InputPort(0));

    impl.set_Tlm_OutputPort(0, tester.get_from_Tlm(0));
    impl.set_Time_OutputPort(0, tester.get_from_Time(0));

    impl.set_Log_OutputPort(0, tester.get_from_Log(0));
    impl.set_LogText_OutputPort(0, tester.get_from_LogText(0));

#if FW_PORT_TRACING
    // Fw::PortBase::setTrace(true);
#endif

    // simpleReg.dump();
}

TEST(CmdDispTestNominal, NominalDispatch) {
    TEST_CASE(102.1.1, "Nominal Dispatch");
    COMMENT("Dispatch a series of commands and verify they are dispatched correctly.");

    Svc::CommandDispatcherImpl impl("CmdDispImpl");

    impl.init(10, 0);

    Svc::CommandDispatcherTester tester(impl);

    tester.init();

    // connect ports
    connectPorts(impl, tester);

    tester.runNominalDispatch();
}

TEST(CmdDispTestNominal, NopTest) {
    TEST_CASE(102.1.2, "NO_OP Command Test");
    COMMENT("Verify the test NO_OP commands by dispatching them.");

    Svc::CommandDispatcherImpl impl("CmdDispImpl");

    impl.init(10, 0);

    Svc::CommandDispatcherTester tester(impl);

    tester.init();

    // connect ports
    connectPorts(impl, tester);

    tester.runNopCommands();
}

TEST(CmdDispTestNominal, ReregisterCommand) {
    TEST_CASE(102.1.3, "Reregister Command");
    COMMENT("Verify user can call command registration port with the same opcode multiple times safely.");

    Svc::CommandDispatcherImpl impl("CmdDispImpl");

    impl.init(10, 0);

    Svc::CommandDispatcherTester tester(impl);

    tester.init();

    // connect ports
    connectPorts(impl, tester);

    tester.runCommandReregister();
}

TEST(CmdDispTestNominal, NonZeroPortDispatch) {
    TEST_CASE(102.1.4, "Nonzero Port Index Dispatch");
    COMMENT("Verify registration and dispatch through a nonzero port index.");

    Svc::CommandDispatcherImpl impl("CmdDispImpl");

    impl.init(10, 0);

    Svc::CommandDispatcherTester tester(impl);

    tester.init();

    // connect ports
    connectPorts(impl, tester);

    // connect a nonzero port index (index 1 is used internally for local commands)
    tester.connect_to_compCmdReg(2, impl.get_compCmdReg_InputPort(2));
    tester.connect_to_seqCmdBuff(2, impl.get_seqCmdBuff_InputPort(2));
    impl.set_compCmdSend_OutputPort(2, tester.get_from_compCmdSend(2));
    impl.set_seqCmdStatus_OutputPort(2, tester.get_from_seqCmdStatus(2));

    tester.runNonZeroPortDispatch();
}

TEST(CmdDispTestOffNominal, InvalidOpcodeDispatch) {
    TEST_CASE(102.2.1, "Off-nominal Dispatch");
    COMMENT("Verify the correct handling of unregistered opcodes.");

    Svc::CommandDispatcherImpl impl("CmdDispImpl");

    impl.init(10, 0);

    Svc::CommandDispatcherTester tester(impl);

    tester.init();

    // connect ports
    connectPorts(impl, tester);

    tester.runInvalidOpcodeDispatch();
}

TEST(CmdDispTestOffNominal, FailedCommand) {
    TEST_CASE(102.2.2, "Off-nominal Failed command");
    COMMENT("Verify that failed commands operate correctly");

    Svc::CommandDispatcherImpl impl("CmdDispImpl");

    impl.init(10, 0);

    Svc::CommandDispatcherTester tester(impl);

    tester.init();

    // connect ports
    connectPorts(impl, tester);

    tester.runFailedCommand();
}

TEST(CmdDispTestOffNominal, InvalidCommand) {
    TEST_CASE(102.2.3, "Off-nominal Invalid Command");
    COMMENT("Verify that malformed commands are detected.");

    Svc::CommandDispatcherImpl impl("CmdDispImpl");

    impl.init(10, 0);

    Svc::CommandDispatcherTester tester(impl);

    tester.init();

    // connect ports
    connectPorts(impl, tester);

    tester.runInvalidCommand();
}

TEST(CmdDispTestOffNominal, CommandOverflow) {
    TEST_CASE(102.2.4, "Off-nominal Command Overflow");
    COMMENT("Verify error case where there are too many outstanding commands.");

    Svc::CommandDispatcherImpl impl("CmdDispImpl");

    impl.init(10, 0);

    Svc::CommandDispatcherTester tester(impl);

    tester.init();

    // connect ports
    connectPorts(impl, tester);

    tester.runOverflowCommands();
}

TEST(CmdDispTestOffNominal, ClearSequenceTracker) {
    TEST_CASE(102.1.3, "Clear Command Tracker");
    COMMENT("Verify command to clear command tracker.");

    Svc::CommandDispatcherImpl impl("CmdDispImpl");

    impl.init(10, 0);

    Svc::CommandDispatcherTester tester(impl);

    tester.init();

    // connect ports
    connectPorts(impl, tester);

    tester.runClearCommandTracking();
}

TEST(CmdDispTestOffNominal, CommandQueueOverflow) {
    TEST_CASE(102.2.5, "Off-nominal Command QueueOverflow");
    COMMENT("Verify error case where the seqCmdBuff port queue overflows and does not ASSERT.");

    Svc::CommandDispatcherImpl impl("CmdDispImpl");

    impl.init(10, 0);

    Svc::CommandDispatcherTester tester(impl);

    tester.init();

    // connect ports
    connectPorts(impl, tester);

    tester.runCommandQueueOverflow();
}

TEST(CmdDispOpcodeMasking, RoundTrip) {
    TEST_CASE(102.3.1, "Opcode Mask Round Trip");
    COMMENT("Verify the opcode masking permutation is invertible over edge and sampled values.");

    const FwOpcodeType edges[] = {0, 1, std::numeric_limits<FwOpcodeType>::max(),
                                  std::numeric_limits<FwOpcodeType>::max() - 1};
    for (FwOpcodeType edge : edges) {
        const FwOpcodeType masked = Svc::CmdDispatcherCfg::maskOpcode(edge);
        ASSERT_EQ(Svc::CmdDispatcherCfg::unmaskOpcode(masked), edge);
    }

    // Sweep pseudo-random opcodes via a linear congruential generator (Numerical Recipes constants)
    FwOpcodeType opcode = 0x12345678 & std::numeric_limits<FwOpcodeType>::max();
    constexpr U32 NUM_SAMPLES = 10000;
    for (U32 i = 0; i < NUM_SAMPLES; i++) {
        opcode = static_cast<FwOpcodeType>(static_cast<U64>(opcode) * 1664525 + 1013904223);
        const FwOpcodeType masked = Svc::CmdDispatcherCfg::maskOpcode(opcode);
        ASSERT_EQ(Svc::CmdDispatcherCfg::unmaskOpcode(masked), opcode);
    }
}

TEST(CmdDispOpcodeMasking, DistinctMaskedValues) {
    TEST_CASE(102.3.2, "Opcode Mask Bijectivity Spot Check");
    COMMENT("Verify a contiguous opcode range maps to distinct masked values.");

    constexpr U32 RANGE = 4096;
    for (U32 i = 0; i < RANGE; i++) {
        for (U32 j = i + 1; j < RANGE; j += 97) {  // stride keeps the check O(n^2/97)
            ASSERT_NE(Svc::CmdDispatcherCfg::maskOpcode(static_cast<FwOpcodeType>(i)),
                      Svc::CmdDispatcherCfg::maskOpcode(static_cast<FwOpcodeType>(j)));
        }
    }
}

TEST(CmdDispOpcodeMasking, GetEventOpcodeDefaultPassthrough) {
    TEST_CASE(102.3.3, "Default Event Opcode Passthrough");
    COMMENT("Verify getEventOpcode passes opcodes through unchanged in the default configuration.");

    static_assert(Svc::CmdDispatcherCfg::IncludeCommandOpcodesInEvents, "Test expects the default configuration");
    static_assert(!Svc::CmdDispatcherCfg::MaskCommandOpcodesInEvents, "Test expects the default configuration");
    const FwOpcodeType samples[] = {0, 1, 0x100, std::numeric_limits<FwOpcodeType>::max()};
    for (FwOpcodeType sample : samples) {
        ASSERT_EQ(Svc::CmdDispatcherCfg::getEventOpcode(sample), sample);
    }
}

#ifndef TGT_OS_TYPE_VXWORKS
int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#endif
