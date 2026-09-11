module Segmented {

  # ----------------------------------------------------------------------
  # Symbolic constants for port numbers
  # ----------------------------------------------------------------------

  enum Ports_RateGroups {
    rateGroup1
    rateGroup2
    rateGroup3
  }

  @ Minimal deployment exercising ComCcsdsSdls.SegmentedSubtopology: the TC Segment Header /
  @ MAP uplink variant with SDLS authentication ahead of MAP reassembly. Selected when the
  @ SEGMENTED_SDLS CMake option is ON. The decryptor is Svc.Ccsds.AesGcmDecryptor (see
  @ ../config/ComCcsdsSdlsConfig.fpp) keyed by sdlsKeyManager from a checked-in TEST key.
  deployment topology Segmented {

    include "SegmentedTopologyCore.fppi"
    include "SegmentedComStackSdls.fppi"

    # ----------------------------------------------------------------------
    # SDLS key management
    # ----------------------------------------------------------------------

    instance sdlsKeyManager

    connections SdlsKey {
      ComCcsdsSdls.decryptor.keyGet -> sdlsKeyManager.keyGet
    }

  }

}
