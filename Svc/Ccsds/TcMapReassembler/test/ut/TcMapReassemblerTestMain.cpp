// ======================================================================
// \title  TcMapReassemblerTestMain.cpp
// \author thomas-bc-autobot
// \brief  cpp file for TcMapReassembler component test main function
// ======================================================================

#include <cstdio>

#include "STest/Random/Random.hpp"
#include "STest/Scenario/BoundedScenario.hpp"
#include "STest/Scenario/RandomScenario.hpp"
#include "Svc/Ccsds/TcMapReassembler/test/ut/TcMapReassemblerTester.hpp"

// ----------------------------------------------------------------------
// Unit tests (DESIGN §8.3)
// ----------------------------------------------------------------------

TEST(Nominal, testUnsegmented) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testUnsegmented();
}

TEST(Nominal, testFirstLast) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testFirstLast();
}

TEST(Nominal, testFirstContLast) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testFirstContLast();
}

TEST(Nominal, testMultiMap) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testMultiMap();
}

TEST(Nominal, testReturnDeallocates) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testReturnDeallocates();
}

TEST(Nominal, testSerializedSize) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testSerializedSize();
}

TEST(OffNominal, testShAbsent) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testShAbsent();
}

TEST(OffNominal, testInvalidMapId) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testInvalidMapId();
}

TEST(OffNominal, testOrphanContinuing) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testOrphanContinuing();
}

TEST(OffNominal, testOrphanLast) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testOrphanLast();
}

TEST(OffNominal, testEmptySegment) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testEmptySegment();
}

TEST(OffNominal, testDuplicateFirst) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testDuplicateFirst();
}

TEST(OffNominal, testUnsegmentedWhileInProgress) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testUnsegmentedWhileInProgress();
}

TEST(OffNominal, testOverflowSegment) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testOverflowSegment();
}

TEST(OffNominal, testOverflowAccumulated) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testOverflowAccumulated();
}

TEST(OffNominal, testFirstDeclaredTooLarge) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testFirstDeclaredTooLarge();
}

TEST(OffNominal, testUnsegmentedDeclaredTooLarge) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testUnsegmentedDeclaredTooLarge();
}

TEST(OffNominal, testAllocFailInvalid) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testAllocFailInvalid();
}

TEST(OffNominal, testAllocFailShort) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testAllocFailShort();
}

TEST(OffNominal, testLengthMismatchLast) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testLengthMismatchLast();
}

TEST(OffNominal, testLengthMismatchUnsegmented) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testLengthMismatchUnsegmented();
}

TEST(OffNominal, testTooShortForHeader) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testTooShortForHeader();
}

TEST(OffNominal, testInFlightBound) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testInFlightBound();
}

TEST(OffNominal, testMacFailureMidPacketModel) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testMacFailureMidPacketModel();
}

TEST(OffNominal, testConfigureAsserts) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    tester.testConfigureAsserts();
}

// ----------------------------------------------------------------------
// Rule-based tests (DESIGN §8.4)
// ----------------------------------------------------------------------

namespace {

//! Every rule applied once in a deterministic order, invariants after each
void applyAllOnce(Svc::Ccsds::TcMapReassemblerTester& tester) {
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendFirst sendFirst;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendContinuing sendContinuing;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendLast sendLast;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendUnsegmented sendUnsegmented;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendOrphan sendOrphan;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendInvalidMap sendInvalidMap;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendEmpty sendEmpty;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendOversize sendOversize;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendShAbsent sendShAbsent;
    Svc::Ccsds::TcMapReassemblerTester::Pool__ReturnPacket returnPacket;
    Svc::Ccsds::TcMapReassemblerTester::Pool__ExhaustPool exhaustPool;

    STest::Rule<Svc::Ccsds::TcMapReassemblerTester>* rules[] = {
        &sendShAbsent, &sendInvalidMap, &sendEmpty,       &sendOrphan,   &sendFirst,    &sendContinuing,
        &sendLast,     &returnPacket,   &sendUnsegmented, &returnPacket, &sendOversize, &exhaustPool,
    };
    for (STest::Rule<Svc::Ccsds::TcMapReassemblerTester>* rule : rules) {
        if (rule->precondition(tester)) {
            rule->apply(tester);
        }
        tester.checkInvariants();
    }
}

}  // namespace

TEST(Rules, applyEachRuleOnce) {
    Svc::Ccsds::TcMapReassemblerTester tester;
    applyAllOnce(tester);
}

TEST(Rules, randomScenario) {
    Svc::Ccsds::TcMapReassemblerTester tester;

    Svc::Ccsds::TcMapReassemblerTester::Segments__SendFirst sendFirst;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendContinuing sendContinuing;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendLast sendLast;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendUnsegmented sendUnsegmented;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendOrphan sendOrphan;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendInvalidMap sendInvalidMap;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendEmpty sendEmpty;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendOversize sendOversize;
    Svc::Ccsds::TcMapReassemblerTester::Segments__SendShAbsent sendShAbsent;
    Svc::Ccsds::TcMapReassemblerTester::Pool__ReturnPacket returnPacket;
    Svc::Ccsds::TcMapReassemblerTester::Pool__ExhaustPool exhaustPool;

    STest::Rule<Svc::Ccsds::TcMapReassemblerTester>* rules[] = {
        &sendFirst, &sendContinuing, &sendLast,     &sendUnsegmented, &sendOrphan,  &sendInvalidMap,
        &sendEmpty, &sendOversize,   &sendShAbsent, &returnPacket,    &exhaustPool,
    };

    // The invariants (DESIGN §8.4, 1-7) are checked after every single step
    struct Checked : public STest::Rule<Svc::Ccsds::TcMapReassemblerTester> {
        explicit Checked(STest::Rule<Svc::Ccsds::TcMapReassemblerTester>& inner)
            : STest::Rule<Svc::Ccsds::TcMapReassemblerTester>(inner.getName()), m_inner(inner) {}
        bool precondition(const Svc::Ccsds::TcMapReassemblerTester& t) override { return m_inner.precondition(t); }
        void action(Svc::Ccsds::TcMapReassemblerTester& t) override {
            m_inner.apply(t);
            t.checkInvariants();
        }
        STest::Rule<Svc::Ccsds::TcMapReassemblerTester>& m_inner;
    };
    Checked checked[] = {
        Checked(sendFirst),    Checked(sendContinuing), Checked(sendLast),    Checked(sendUnsegmented),
        Checked(sendOrphan),   Checked(sendInvalidMap), Checked(sendEmpty),   Checked(sendOversize),
        Checked(sendShAbsent), Checked(returnPacket),   Checked(exhaustPool),
    };
    STest::Rule<Svc::Ccsds::TcMapReassemblerTester>* checkedRules[FW_NUM_ARRAY_ELEMENTS(rules)];
    for (FwSizeType i = 0; i < FW_NUM_ARRAY_ELEMENTS(rules); i++) {
        checkedRules[i] = &checked[i];
    }

    const U32 numRulesToApply = 10000;
    STest::RandomScenario<Svc::Ccsds::TcMapReassemblerTester> random("Random Rules", checkedRules,
                                                                     FW_NUM_ARRAY_ELEMENTS(checkedRules));
    STest::BoundedScenario<Svc::Ccsds::TcMapReassemblerTester> bounded("Bounded Random Rules Scenario", random,
                                                                       numRulesToApply);
    const U32 numSteps = bounded.run(tester);
    ASSERT_EQ(numSteps, numRulesToApply);
    tester.checkInvariants();
    // Every terminal outcome of DESIGN §5.2/§5.3 must have been exercised at least once
    EXPECT_GT(tester.shadow_packetsReassembled, 0U);
    EXPECT_GT(tester.shadow_events.segmentHeaderAbsent, 0U);
    EXPECT_GT(tester.shadow_events.invalidMapId, 0U);
    EXPECT_GT(tester.shadow_events.unexpectedSegment, 0U);
    EXPECT_GT(tester.shadow_events.packetAbandoned, 0U);
    EXPECT_GT(tester.shadow_events.emptySegment, 0U);
    EXPECT_GT(tester.shadow_events.packetTooLarge, 0U);
    EXPECT_GT(tester.shadow_events.allocationFailed, 0U);
    EXPECT_GT(tester.shadow_events.lengthMismatch, 0U);
    printf(
        "[  STest   ] reassembled=%u dropped=%u abandoned=%u shAbsent=%u invalidMap=%u orphan=%u empty=%u tooLarge=%u "
        "allocFail=%u mismatch=%u\n",
        tester.shadow_packetsReassembled, tester.shadow_segmentsDropped, tester.shadow_packetsAbandoned,
        tester.shadow_events.segmentHeaderAbsent, tester.shadow_events.invalidMapId,
        tester.shadow_events.unexpectedSegment, tester.shadow_events.emptySegment, tester.shadow_events.packetTooLarge,
        tester.shadow_events.allocationFailed, tester.shadow_events.lengthMismatch);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}
