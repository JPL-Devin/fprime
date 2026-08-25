// ======================================================================
// \title  CcsdsUslpFrameDetector.hpp
// \author thomas-bc
// \brief  hpp file for CCSDS USLP frame detector definitions
// ======================================================================
#ifndef SVC_FRAME_ACCUMULATOR_FRAME_DETECTOR_CCSDS_USLP_FRAME_DETECTOR
#define SVC_FRAME_ACCUMULATOR_FRAME_DETECTOR_CCSDS_USLP_FRAME_DETECTOR

#include "Fw/FPrimeBasicTypes.hpp"
#include "Svc/Ccsds/Types/FppConstantsAc.hpp"
#include "Svc/FrameAccumulator/FrameDetector.hpp"

namespace Svc {
namespace FrameDetectors {

//! \brief CCSDS USLP uplink transfer frame detector
class CcsdsUslpFrameDetector : public FrameDetector {
  public:
    //! \brief detect if a frame is available within the circular buffer
    //!
    //! Function implemented by sub classes used to determine if a frame is available at the current position of the
    //! circular buffer. Implementors should detect if a frame is available, set size_out, and return a status while
    //! following these expectations:
    //!
    //!  1. FRAME_DETECTED status implies a frame is available at the current offset of the circular buffer.
    //!     size_out must be set to the size of the frame from that location.
    //!
    //!  2. NO_FRAME_DETECTED status implies no frame is possible at the current offset of the circular buffer.
    //!     e.g. no start word is found at the current offset. size_out is ignored.
    //!
    //!  3. MORE_DATA_NEEDED status implies that a frame might be possible but more data is needed before a
    //!     determination is possible. size_out must be set to the total amount of data needed.
    //!
    //! \param data: circular buffer with read-only access
    //! \param size_out: set as output to caller indicating size when appropriate
    //! \return status of the detection to be paired with size_out
    Status detect(const Types::CircularBuffer& data, FwSizeType& size_out) const override;

  protected:
    //! \brief mask selecting the constant fields of the first USLP header word (TFVN and Spacecraft ID)
    //! The VCID and MAP ID vary per frame and are therefore not part of the sync token
    const U32 m_expectedTokenMask =
        Ccsds::USLPHeaderSubfields::frameVersionMask | static_cast<U32>(Ccsds::USLPHeaderSubfields::spacecraftIdMask);

    //! \brief expected TFVN and spacecraft ID token for a valid USLP frame
    const U32 m_expectedHeaderToken =
        (static_cast<U32>(Ccsds::USLPHeaderSubfields::frameVersionValue)
         << Ccsds::USLPHeaderSubfields::frameVersionOffset) |
        (static_cast<U32>(ComCfg::SpacecraftId) << Ccsds::USLPHeaderSubfields::spacecraftIdOffset);

};  // class CcsdsUslpFrameDetector
}  // namespace FrameDetectors
}  // namespace Svc

#endif  // SVC_FRAME_ACCUMULATOR_FRAME_DETECTOR_CCSDS_USLP_FRAME_DETECTOR
