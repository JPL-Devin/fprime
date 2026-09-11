// ======================================================================
// \title  SegmentedTopologyDefs.hpp
// \brief required header for the Segmented topology autocoder
// ======================================================================
#ifndef SEGMENTED_SEGMENTEDTOPOLOGYDEFS_HPP
#define SEGMENTED_SEGMENTEDTOPOLOGYDEFS_HPP

#include "Fw/Types/Assert.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include "SubtopologyBuilds/Segmented/Top/FppConstantsAc.hpp"

// Subtopology PingEntries includes
#include "Svc/Subtopologies/CdhCore/PingEntries.hpp"
#include "Svc/Subtopologies/ComCcsds/PingEntries.hpp"
#include "Svc/Subtopologies/FileHandling/PingEntries.hpp"

// Subtopology TopologyDefs includes
#include "Svc/Subtopologies/CdhCore/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/ComCcsds/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/FileHandling/SubtopologyTopologyDefs.hpp"

// Health ping entries: missed-ping counts before WARNING_HI / FATAL
namespace PingEntries {
namespace Segmented_rateGroup1Comp {
enum { WARN = 3, FATAL = 5 };
}
namespace Segmented_rateGroup2Comp {
enum { WARN = 3, FATAL = 5 };
}
namespace Segmented_rateGroup3Comp {
enum { WARN = 3, FATAL = 5 };
}
}  // namespace PingEntries

// Definitions are placed within a namespace named after the deployment
namespace Segmented {

//! State carried from the command line into the autocoded topology phases
struct TopologyState {
    const char* hostname;                         //!< Hostname for TCP communication
    U16 port;                                     //!< Port for TCP communication
    const char* sdlsKeyFile;                      //!< Path to the SDLS key file (SDLS variant only)
    CdhCore::SubtopologyState cdhCore;            //!< Subtopology state for CdhCore
    ComCcsds::SubtopologyState comCcsds;          //!< Subtopology state for ComCcsds
    FileHandling::SubtopologyState fileHandling;  //!< Subtopology state for FileHandling
};

namespace PingEntries = ::PingEntries;
}  // namespace Segmented

#endif
