// ======================================================================
// \title  MyFSWTopology.cpp
// \brief  cpp file containing the topology instantiation code
// ======================================================================

// Provides access to autocoded functions
#include <MyFSW/Top/MyFSWTopologyAc.hpp>
#include <MyFSW/Top/MyFSWTopology.hpp>

// Necessary project-specified types
#include <Fw/Types/MallocAllocator.hpp>

// Allows easy reference to objects in FPP/autocoder required namespaces
using namespace MyFSW;

// Instantiate a malloc allocator for cmdSeq buffer allocation
Fw::MallocAllocator mallocator;

// The MyFSW topology divides the incoming clock signal (1Hz) into sub-signals:
// 1Hz and 1/2Hz with zero offset
Svc::RateGroupDriver::DividerSet rateGroupDivisorsSet{{{1, 0}, {2, 0}}};

// Rate group context tokens (unused in this project, set to zero)
U32 rateGroup1Context[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {};
U32 rateGroup2Context[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {};

enum TopologyConstants {
    COMM_PRIORITY = 34,
};

/**
 * \brief Configure components in project-specific way
 *
 * This helper function configures each component requiring project-specific
 * input, including allocating resources and passing arguments.
 */
void configureTopology() {
    // Rate group driver needs a divisor list
    rateGroupDriverComp.configure(rateGroupDivisorsSet);

    // Rate groups require context arrays
    rateGroup1Comp.configure(rateGroup1Context, FW_NUM_ARRAY_ELEMENTS(rateGroup1Context));
    rateGroup2Comp.configure(rateGroup2Context, FW_NUM_ARRAY_ELEMENTS(rateGroup2Context));

    // Command sequencer needs memory for command sequences
    cmdSeq.allocateBuffer(0, mallocator, 5 * 1024);
}

// Public functions for use in main program
namespace MyFSW {

void setupTopology(const TopologyState& state) {
    // Autocoded initialization
    initComponents(state);
    // Autocoded id setup
    setBaseIds();
    // Autocoded connection wiring
    connectComponents();
    // Autocoded command registration
    regCommands();
    // Autocoded configuration
    configComponents(state);

    // Configure TCP client if hostname/port provided
    if (state.hostname != nullptr && state.port != 0) {
        comDriver.configure(state.hostname, state.port);
    }

    // Project-specific component configuration
    configureTopology();

    // Autocoded parameter loading
    loadParameters();
    // Autocoded task kick-off (active components)
    startTasks(state);

    // Start socket client communication if configured
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
    // Autocoded task clean-up
    stopTasks(state);
    freeThreads(state);

    // Stop the comDriver component
    comDriver.stop();
    (void)comDriver.join();

    // Resource deallocation
    cmdSeq.deallocateBuffer(mallocator);
    tearDownComponents(state);
}

}  // namespace MyFSW
