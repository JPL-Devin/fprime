module Ref {

  include "RefTopologyDefs.fppi"

  # Ref deployment with the segmented TC uplink (ComCcsds.SegmentedSubtopology:
  # Segment Header / MAP packet reassembly). Selected with -DREF_TC_SEGMENTED=ON;
  # the default deployment is topology.fpp. Same topology name so that the generated
  # RefTopologyAc.* and RefTopologyDictionary.json keep their names.
  deployment topology Ref {
    include "RefTopologyCore.fppi"

    include "RefComStackSegmented.fppi"

    include "RefPacketsSegmented.fppi"
  }

}
