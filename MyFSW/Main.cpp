// ======================================================================
// \title  Main.cpp
// \brief  main program for MyFSW flight software application
// ======================================================================

#include <MyFSW/Top/MyFSWTopology.hpp>
#include <signal.h>
#include <getopt.h>
#include <cstdlib>
#include <Os/Os.hpp>

/**
 * \brief Print command line help message
 */
void print_usage(const char* app) {
    (void)printf("Usage: ./%s [options]\n-a\thostname/IP address\n-p\tport_number\n", app);
}

/**
 * \brief Signal handler to shutdown topology cycling
 */
static void signalHandler(int signum) {
    MyFSW::stopRateGroups();
}

/**
 * \brief Main entry point for MyFSW flight software
 *
 * This program is designed to run in standard environments (e.g. Linux/macOS).
 * It uses command line inputs to specify how to connect to the ground system.
 */
int main(int argc, char* argv[]) {
    Os::init();
    U16 port_number = 0;
    I32 option = 0;
    char* hostname = nullptr;

    // Parse command line arguments
    while ((option = getopt(argc, argv, "hp:a:")) != -1) {
        switch (option) {
            case 'a':
                hostname = optarg;
                break;
            case 'p':
                port_number = static_cast<U16>(atoi(optarg));
                break;
            case 'h':
            case '?':
            default:
                print_usage(argv[0]);
                return (option == 'h') ? 0 : 1;
        }
    }

    // Build topology state
    MyFSW::TopologyState inputs;
    inputs.hostname = hostname;
    inputs.port = port_number;

    // Setup signal handlers for clean shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    (void)printf("MyFSW Flight Software starting...\n");
    (void)printf("Hit Ctrl-C to quit\n");

    // Setup, cycle, and teardown topology
    MyFSW::setupTopology(inputs);
    MyFSW::startRateGroups(Fw::TimeInterval(1, 0));  // 1Hz rate group cycling
    MyFSW::teardownTopology(inputs);
    (void)printf("MyFSW exiting...\n");
    return 0;
}
