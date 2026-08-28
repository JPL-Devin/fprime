// ======================================================================
// \title  MyFSWTopologyDefs.hpp
// \brief  Required definitions for the topology autocoder
// ======================================================================

#ifndef MYFSW_TOPOLOGYDEFS_HPP
#define MYFSW_TOPOLOGYDEFS_HPP

#include "MyFSW/Top/FppConstantsAc.hpp"

// Subtopology PingEntries includes
#include "Svc/Subtopologies/CdhCore/PingEntries.hpp"
#include "Svc/Subtopologies/ComFprime/PingEntries.hpp"
#include "Svc/Subtopologies/FileHandling/PingEntries.hpp"

// SubtopologyTopologyDefs includes
#include "Svc/Subtopologies/CdhCore/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/ComFprime/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/FileHandling/SubtopologyTopologyDefs.hpp"

// ComFprime Enum Includes
#include "Svc/Subtopologies/ComFprime/Ports_ComBufferQueueEnumAc.hpp"
#include "Svc/Subtopologies/ComFprime/Ports_ComPacketQueueEnumAc.hpp"

/**
 * \brief Required ping constants
 *
 * The topology autocoder requires WARN and FATAL constant definitions for each
 * component that supports the health-ping interface. These specify how many
 * missed pings trigger WARNING_HI/FATAL events.
 */
namespace PingEntries {
namespace MyFSW_thermalController {
enum { WARN = 3, FATAL = 5 };
}
namespace MyFSW_rateGroup1Comp {
enum { WARN = 3, FATAL = 5 };
}
namespace MyFSW_rateGroup2Comp {
enum { WARN = 3, FATAL = 5 };
}
namespace MyFSW_cmdSeq {
enum { WARN = 3, FATAL = 5 };
}
}  // namespace PingEntries

namespace MyFSW {

/**
 * \brief Required TopologyState type
 *
 * The topology autocoder requires an object that carries state with the name
 * MyFSW::TopologyState. The contents are project-specific.
 */
struct TopologyState {
    const char* hostname;                          //!< Hostname for TCP communication
    U16 port;                                      //!< Port for TCP communication
    CdhCore::SubtopologyState cdhCore;             //!< Subtopology state for CdhCore
    ComFprime::SubtopologyState comFprime;         //!< Subtopology state for ComFprime
    FileHandling::SubtopologyState fileHandling;   //!< Subtopology state for FileHandling
};

namespace PingEntries = ::PingEntries;

}  // namespace MyFSW

#endif
