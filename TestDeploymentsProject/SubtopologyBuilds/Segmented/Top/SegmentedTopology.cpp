// ======================================================================
// \title  SegmentedTopology.cpp
// \brief cpp file containing the topology instantiation code
// ======================================================================

#include <SubtopologyBuilds/Segmented/Top/SegmentedTopology.hpp>
#include <SubtopologyBuilds/Segmented/Top/SegmentedTopologyAc.hpp>

using namespace Segmented;

// 1Hz input clock divided into 1Hz, 1/2Hz and 1/4Hz rate groups, zero offset
static Svc::RateGroupDriver::DividerSet rateGroupDivisorsSet{{{1, 0}, {2, 0}, {4, 0}}};

static Svc::ActiveRateGroup::ContextArray rateGroup1Context(0);
static Svc::ActiveRateGroup::ContextArray rateGroup2Context(0);
static Svc::ActiveRateGroup::ContextArray rateGroup3Context(0);

enum TopologyConstants {
    COMM_PRIORITY = 34,
};

static void configureTopology() {
    rateGroupDriverComp.configure(rateGroupDivisorsSet);
    rateGroup1Comp.configure(rateGroup1Context);
    rateGroup2Comp.configure(rateGroup2Context);
    rateGroup3Comp.configure(rateGroup3Context);

    // Restrict file access to the working directory (where PrmDb.dat lives)
    FileHandling::fileUplink.configure(".");
    FileHandling::fileDownlink.configure(".");
    FileHandling::prmDb.configureSandbox(".");
}

namespace Segmented {

void setupTopology(const TopologyState& state) {
    initComponents(state);
    setBaseIds();
    connectComponents();
    configComponents(state);
    if (state.hostname != nullptr && state.port != 0) {
        comDriver.configure(state.hostname, state.port);
    }
    configureTopology();
    regCommands();
    readParameters();
    loadParameters();
    startTasks(state);
    if (state.hostname != nullptr && state.port != 0) {
        Os::TaskString name("ReceiveTask");
        comDriver.start(name, COMM_PRIORITY, Default::STACK_SIZE);
    }
}

void startRateGroups(const Fw::TimeInterval& interval) {
    linuxTimer.startTimer(interval);
}

void stopRateGroups() {
    linuxTimer.quit();
}

void teardownTopology(const TopologyState& state) {
    stopTasks(state);
    freeThreads(state);
    comDriver.stop();
    (void)comDriver.join();
    tearDownComponents(state);
    deinitComponents(state);
}

}  // namespace Segmented
