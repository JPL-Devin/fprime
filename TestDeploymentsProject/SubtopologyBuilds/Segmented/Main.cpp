// ======================================================================
// \title  Main.cpp
// \brief main program for the Segmented test deployment (Linux, macOS)
// ======================================================================
#include <getopt.h>
#include <signal.h>
#include <Fw/Logger/Logger.hpp>
#include <Os/Os.hpp>
#include <SubtopologyBuilds/Segmented/Top/SegmentedTopology.hpp>
#include <cstdlib>

static void print_usage(const char* app) {
    Fw::Logger::log(
        "Usage: ./%s [options]\n-a\thostname/IP address\n-p\tport_number\n-k, --sdls-key-file\tSDLS key file (SDLS "
        "build only)\n",
        app);
}

static void signalHandler(int signum) {
    Segmented::stopRateGroups();
}

int main(int argc, char* argv[]) {
    Os::init();
    U16 port_number = 0;
    I32 option = 0;
    char* hostname = nullptr;
    char* sdlsKeyFile = nullptr;

    static const struct option longOptions[] = {{"sdls-key-file", required_argument, nullptr, 'k'},
                                                {nullptr, 0, nullptr, 0}};
    while ((option = getopt_long(argc, argv, "hp:a:k:", longOptions, nullptr)) != -1) {
        switch (option) {
            case 'a':
                hostname = optarg;
                break;
            case 'p':
                port_number = static_cast<U16>(atoi(optarg));
                break;
            case 'k':
                sdlsKeyFile = optarg;
                break;
            case 'h':
            case '?':
            default:
                print_usage(argv[0]);
                return (option == 'h') ? 0 : 1;
        }
    }
#ifdef SEGMENTED_SDLS
    if (sdlsKeyFile == nullptr) {
        Fw::Logger::log("SDLS build requires -k <key file>\n");
        print_usage(argv[0]);
        return 1;
    }
#endif
    Segmented::TopologyState inputs;
    inputs.hostname = hostname;
    inputs.port = port_number;
    inputs.sdlsKeyFile = sdlsKeyFile;

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    Fw::Logger::log("Hit Ctrl-C to quit\n");

    Segmented::setupTopology(inputs);
    Segmented::startRateGroups(Fw::TimeInterval(1, 0));
    Segmented::teardownTopology(inputs);
    Fw::Logger::log("Exiting...\n");
    return 0;
}
