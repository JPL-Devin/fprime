module Segmented {

  # ----------------------------------------------------------------------
  # Symbolic constants for port numbers
  # ----------------------------------------------------------------------

  enum Ports_RateGroups {
    rateGroup1
    rateGroup2
    rateGroup3
  }

  @ Minimal deployment exercising ComCcsds.SegmentedSubtopology: the TC Segment Header /
  @ MAP uplink variant (tcDeframerSeg -> tcMapReassembler) without SDLS. Selected when the
  @ SEGMENTED_SDLS CMake option is OFF; topologySdls.fpp is used when it is ON.
  deployment topology Segmented {

    include "SegmentedTopologyCore.fppi"
    include "SegmentedComStack.fppi"

  }

}
