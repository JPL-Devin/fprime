// ======================================================================
// \title  TcDeframer.hpp
// \author thomas-bc
// \brief  hpp file for TcDeframer component implementation class
// ======================================================================

#ifndef Svc_Ccsds_TcDeframer_HPP
#define Svc_Ccsds_TcDeframer_HPP

#include "Svc/Ccsds/TcDeframer/TcDeframerComponentAc.hpp"

namespace Svc {
namespace Ccsds {
class TcDeframer : public TcDeframerComponentBase {
    friend class TcDeframerTester;

  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct TcDeframer object
    TcDeframer(const char* const compName  //!< The component name
    );

    //! Destroy TcDeframer object
    ~TcDeframer();

    //! \brief Configure the TcDeframer to deframe only a specific VCID and spacecraft ID
    //!
    //! By default, the TcDeframer is configured with the spacecraft ID set in the config/ComCfg.fpp file,
    //! and deframes all incoming frames regardless of their VCID. Should project instantiate a TcDeframer
    //! with a different configuration, they can use this configure method to set the desired properties.
    //!
    //! \param vcId The virtual channel ID to accept (if acceptAllVcid is false)
    //! \param spacecraftId The spacecraft ID to accept
    //! \param acceptAllVcid If true, the deframer will accept all VCIDs. If false, it will only accept configured vcId
    //!
    void configure(U16 vcId, U16 spacecraftId, bool acceptAllVcid);

    //! \brief Enable or disable TC Segment Header processing (CCSDS 232.0-B-4 4.1.3.2.2)
    //!
    //! When enabled, every accepted Type-BD frame must carry a one-octet Segment Header immediately after the
    //! primary header. The octet is stripped from the emitted data and copied verbatim into the emitted
    //! `ComCfg::FrameContext` (`tcSegmentHeaderPresent = true`, `tcSegmentHeader = octet`). Type-BC/AC control
    //! frames, which carry no Segment Header, are dropped. Call once during component configuration; the default
    //! (disabled) preserves the baseline behaviour byte-for-byte.
    //!
    //! \param enabled true to enable Segment Header mode, false to disable it
    //!
    void configureSegmentHeader(bool enabled);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //!
    //! Port to receive framed data
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& data,
                        const ComCfg::FrameContext& context) override;

    //! Handler implementation for dataReturnIn
    //!
    //! Port receiving back ownership of sent frame buffers
    void dataReturnIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& data,     //!< The buffer
                              const ComCfg::FrameContext& context) override;

    //! Helper method to send an error notification if the errorNotify port is connected
    //! \param error The error to send
    void errorNotifyHelper(Svc::Ccsds::FrameError error);

  private:
    U16 m_vcId;                   //!< The virtual channel ID this deframer is configured to handle
    U16 m_spacecraftId;           //!< The spacecraft ID this deframer is configured to handle
    bool m_acceptAllVcid = true;  //!< Flag to accept all VCIDs
    bool m_segmentHeaderEnabled;  //!< Segment Header mode (CCSDS 232.0-B-4 4.1.3.2.2); false by default
};
}  // namespace Ccsds
}  // namespace Svc

#endif
