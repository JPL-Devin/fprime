// ======================================================================
// \title  UslpDeframer.hpp
// \author thomas-bc
// \brief  hpp file for UslpDeframer component implementation class
// ======================================================================

#ifndef Svc_Ccsds_UslpDeframer_HPP
#define Svc_Ccsds_UslpDeframer_HPP

#include "Svc/Ccsds/UslpDeframer/UslpDeframerComponentAc.hpp"

namespace Svc {
namespace Ccsds {
class UslpDeframer : public UslpDeframerComponentBase {
    friend class UslpDeframerTester;

  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct UslpDeframer object
    UslpDeframer(const char* const compName  //!< The component name
    );

    //! Destroy UslpDeframer object
    ~UslpDeframer();

    //! \brief Configure the UslpDeframer with frame identification and acceptance parameters
    //!
    //! By default, the UslpDeframer is configured with the spacecraft ID set in the config/ComCfg.fpp file,
    //! VCID 0, MAP ID 0, a VCF Count Length of 0 octets, and only accepts the configured VCID.
    //!
    //! \param vcId The virtual channel ID to accept (6 bits, if acceptAllVcid is false)
    //! \param spacecraftId The spacecraft ID to accept (16 bits)
    //! \param mapId The MAP ID to accept (4 bits)
    //! \param vcfCountLength The expected VCF Count field length in octets (0-7)
    //! \param acceptAllVcid If true, the deframer will accept all VCIDs. If false, it will only accept configured vcId
    //!
    void configure(U8 vcId, U16 spacecraftId, U8 mapId, U8 vcfCountLength, bool acceptAllVcid);

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
    U8 m_vcId;              //!< The virtual channel ID this deframer is configured to handle
    U16 m_spacecraftId;     //!< The spacecraft ID this deframer is configured to handle
    U8 m_mapId;             //!< The MAP ID this deframer is configured to handle
    U8 m_vcfCountLength;    //!< The expected VCF Count field length in octets (0-7)
    bool m_acceptAllVcid;   //!< Flag to accept all VCIDs
    U32 m_framesProcessed;  //!< Count of successfully deframed frames
    U32 m_crcErrorCount;    //!< Count of FECF (CRC) errors
};
}  // namespace Ccsds
}  // namespace Svc

#endif
