module Ref {

  include "RefTopologyDefs.fppi"

  # Default Ref deployment: standard CCSDS uplink (ComCcsds.Subtopology). The
  # segmented-uplink variant is topologySegmented.fpp, selected with -DREF_TC_SEGMENTED=ON.
  deployment topology Ref {
    include "RefTopologyCore.fppi"

    include "RefComStack.fppi"

    include "RefPackets.fppi"
  }

}
