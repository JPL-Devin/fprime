// ======================================================================
// \title  MyFSWTopology.hpp
// \brief  hpp file containing topology setup/teardown declarations
// ======================================================================

#ifndef MYFSW_TOPOLOGY_HPP
#define MYFSW_TOPOLOGY_HPP

#include "MyFSW/Top/MyFSWTopologyDefs.hpp"
#include <Fw/Time/TimeInterval.hpp>

namespace MyFSW {

/**
 * \brief Setup the MyFSW topology
 *
 * Instantiates, configures, and connects all components. Starts active
 * component tasks.
 */
void setupTopology(const TopologyState& state);

/**
 * \brief Start the rate groups
 *
 * Starts the Linux timer that drives the rate group driver. This call blocks
 * until stopRateGroups() is called (e.g., from a signal handler).
 */
void startRateGroups(const Fw::TimeInterval& interval);

/**
 * \brief Stop the rate groups
 *
 * Signals the Linux timer to quit, unblocking startRateGroups().
 */
void stopRateGroups();

/**
 * \brief Teardown the MyFSW topology
 *
 * Stops tasks, joins threads, and deallocates resources.
 */
void teardownTopology(const TopologyState& state);

}  // namespace MyFSW

#endif
