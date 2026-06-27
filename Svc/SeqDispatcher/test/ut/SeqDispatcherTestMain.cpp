// ----------------------------------------------------------------------
// TestMain.cpp
// ----------------------------------------------------------------------

#include "STest/Random/Random.hpp"
#include "STest/Scenario/BoundedScenario.hpp"
#include "STest/Scenario/RandomScenario.hpp"
#include "SeqDispatcherTester.hpp"

// ----------------------------------------------------------------------
// Existing Tests
// ----------------------------------------------------------------------

TEST(Nominal, testDispatch) {
    Svc::SeqDispatcherTester tester;
    tester.testDispatch();
}

TEST(Nominal, testLogStatus) {
    Svc::SeqDispatcherTester tester;
    tester.testLogStatus();
}

TEST(RunArgs, testRunArgsWithValidArguments) {
    Svc::SeqDispatcherTester tester;
    tester.testRunArgsWithValidArguments();
}

TEST(RunArgs, testRunArgsWithMaxSizedArguments) {
    Svc::SeqDispatcherTester tester;
    tester.testRunArgsWithMaxSizedArguments();
}

TEST(RunArgs, testRunArgsNoSequencersAvailable) {
    Svc::SeqDispatcherTester tester;
    tester.testRunArgsNoSequencersAvailable();
}

TEST(RunArgs, testRunArgsBlockingVsNonBlocking) {
    Svc::SeqDispatcherTester tester;
    tester.testRunArgsBlockingVsNonBlocking();
}

// ----------------------------------------------------------------------
// Rules-Based Tests
// ----------------------------------------------------------------------

using Svc::SeqDispatcherTester;

TEST(RulesBased, RunCmdOk) {
    SeqDispatcherTester tester;
    SeqDispatcherTester::RunCmd__Ok rule;
    rule.apply(tester);
}

TEST(RulesBased, RunCmdNoSequencersAvailable) {
    SeqDispatcherTester tester;
    // Fill all sequencers first
    SeqDispatcherTester::RunCmd__Ok fillRule;
    for (int i = 0; i < SeqDispatcherSequencerPorts; i++) {
        fillRule.apply(tester);
    }
    SeqDispatcherTester::RunCmd__NoSequencersAvailable rule;
    rule.apply(tester);
}

TEST(RulesBased, SeqDoneKnownBlock) {
    SeqDispatcherTester tester;
    SeqDispatcherTester::RunCmd__Ok runRule;
    SeqDispatcherTester::SeqDone__KnownNoBlock freeRule;
    SeqDispatcherTester::SeqDone__KnownBlock doneRule;
    // Keep applying until we get a blocking sequencer, freeing NO_BLOCK ones if needed
    while (!tester.shadow.shadow_hasBlockingSequencer()) {
        if (!tester.shadow.shadow_hasAvailableSequencer() && tester.shadow.shadow_hasNonBlockingSequencer()) {
            freeRule.apply(tester);
        }
        runRule.apply(tester);
    }
    doneRule.apply(tester);
}

TEST(RulesBased, SeqDoneKnownNoBlock) {
    SeqDispatcherTester tester;
    SeqDispatcherTester::RunCmd__Ok runRule;
    SeqDispatcherTester::SeqDone__KnownBlock freeRule;
    SeqDispatcherTester::SeqDone__KnownNoBlock doneRule;
    // Keep applying until we get a non-blocking sequencer, freeing BLOCK ones if needed
    while (!tester.shadow.shadow_hasNonBlockingSequencer()) {
        if (!tester.shadow.shadow_hasAvailableSequencer() && tester.shadow.shadow_hasBlockingSequencer()) {
            freeRule.apply(tester);
        }
        runRule.apply(tester);
    }
    doneRule.apply(tester);
}

TEST(RulesBased, SeqDoneUnknown) {
    SeqDispatcherTester tester;
    SeqDispatcherTester::SeqDone__Unknown rule;
    rule.apply(tester);
}

TEST(RulesBased, SeqStartExpected) {
    SeqDispatcherTester tester;
    SeqDispatcherTester::RunCmd__Ok runRule;
    SeqDispatcherTester::SeqStart__Expected startRule;
    runRule.apply(tester);
    startRule.apply(tester);
}

TEST(RulesBased, SeqStartUnexpected) {
    SeqDispatcherTester tester;
    SeqDispatcherTester::SeqStart__Unexpected rule;
    rule.apply(tester);
}

TEST(RulesBased, SeqStartConflicting) {
    SeqDispatcherTester tester;
    SeqDispatcherTester::RunCmd__Ok runRule;
    SeqDispatcherTester::SeqStart__Conflicting conflictRule;
    runRule.apply(tester);
    conflictRule.apply(tester);
}

TEST(RulesBased, SeqRunInOk) {
    SeqDispatcherTester tester;
    SeqDispatcherTester::SeqRunIn__Ok rule;
    rule.apply(tester);
}

TEST(RulesBased, SeqRunInNoSequencersAvailable) {
    SeqDispatcherTester tester;
    SeqDispatcherTester::RunCmd__Ok fillRule;
    for (int i = 0; i < SeqDispatcherSequencerPorts; i++) {
        fillRule.apply(tester);
    }
    SeqDispatcherTester::SeqRunIn__NoSequencersAvailable rule;
    rule.apply(tester);
}

TEST(RulesBased, LogStatusCmd) {
    SeqDispatcherTester tester;
    SeqDispatcherTester::LogStatusCmd__Ok rule;
    rule.apply(tester);
}

TEST(RulesBased, LogStatusCmdWithRunning) {
    SeqDispatcherTester tester;
    SeqDispatcherTester::RunCmd__Ok runRule;
    SeqDispatcherTester::LogStatusCmd__Ok logRule;
    runRule.apply(tester);
    logRule.apply(tester);
}

TEST(RulesBased, RandomizedTesting) {
    U32 numRulesToApply = 10000;
    SeqDispatcherTester tester;

    SeqDispatcherTester::RunCmd__Ok ruleRunOk;
    SeqDispatcherTester::RunCmd__NoSequencersAvailable ruleRunNoSeq;
    SeqDispatcherTester::SeqDone__KnownBlock ruleDoneBlock;
    SeqDispatcherTester::SeqDone__KnownNoBlock ruleDoneNoBlock;
    SeqDispatcherTester::SeqDone__Unknown ruleDoneUnknown;
    SeqDispatcherTester::SeqStart__Expected ruleStartExpected;
    SeqDispatcherTester::SeqStart__Unexpected ruleStartUnexpected;
    SeqDispatcherTester::SeqStart__Conflicting ruleStartConflicting;
    SeqDispatcherTester::SeqRunIn__Ok ruleRunInOk;
    SeqDispatcherTester::SeqRunIn__NoSequencersAvailable ruleRunInNoSeq;
    SeqDispatcherTester::LogStatusCmd__Ok ruleLogStatus;

    STest::Rule<SeqDispatcherTester>* rules[] = {
        &ruleRunOk,       &ruleRunNoSeq,      &ruleDoneBlock,       &ruleDoneNoBlock,
        &ruleDoneUnknown, &ruleStartExpected, &ruleStartUnexpected, &ruleStartConflicting,
        &ruleRunInOk,     &ruleRunInNoSeq,    &ruleLogStatus,
    };

    STest::RandomScenario<SeqDispatcherTester> random("Random Rules", rules, FW_NUM_ARRAY_ELEMENTS(rules));
    STest::BoundedScenario<SeqDispatcherTester> bounded("Bounded Random Rules Scenario", random, numRulesToApply);
    const U32 numSteps = bounded.run(tester);
    printf("Ran %u steps.\n", numSteps);
}

int main(int argc, char** argv) {
    STest::Random::seed();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
