// ======================================================================
// \title  SegmentedTopology.hpp
// \brief header file containing the topology instantiation definitions
// ======================================================================
#ifndef SEGMENTED_SEGMENTEDTOPOLOGY_HPP
#define SEGMENTED_SEGMENTEDTOPOLOGY_HPP

#include <Fw/Time/TimeInterval.hpp>
#include <SubtopologyBuilds/Segmented/Top/SegmentedTopologyDefs.hpp>

namespace Segmented {

//! Initialize, configure, connect and start the topology (see Ref::setupTopology for the step order)
void setupTopology(const TopologyState& state);

//! Stop and tear down the topology
void teardownTopology(const TopologyState& state);

//! Cycle the rate group driver from the Linux timer; blocks until stopRateGroups() is called
void startRateGroups(const Fw::TimeInterval& interval);

//! Stop the rate group cycling started by startRateGroups()
void stopRateGroups();

}  // namespace Segmented

#endif
