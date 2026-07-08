// ======================================================================
// \title  SdlsSaRouter.hpp
// \author lestarch-autobot
// \brief  hpp file for SdlsSaRouter component implementation class
// ======================================================================

#ifndef Svc_Ccsds_SdlsSaRouter_HPP
#define Svc_Ccsds_SdlsSaRouter_HPP

#include "Svc/Ccsds/SdlsSaRouter/SdlsSaRouterComponentAc.hpp"

namespace Svc {

namespace Ccsds {

class SdlsSaRouter final : public SdlsSaRouterComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SdlsSaRouter object
    SdlsSaRouter(const char* const compName  //!< The component name
    );

    //! Destroy SdlsSaRouter object
    ~SdlsSaRouter();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for decryptIn
    //!
    //! Port to receive the security association index and iv/data buffer to decrypt
    Svc::Ccsds::SdlsStatus decryptIn_handler(FwIndexType portNum,  //!< The port number
                                             U16 securityAssociationIndex,
                                             Fw::Buffer& data) override;

    //! Handler implementation for decryptReturnIn
    //!
    //! Port for receiving back ownership of buffers sent on decryptOut
    void decryptReturnIn_handler(FwIndexType portNum,  //!< The port number
                                 Fw::Buffer& fwBuffer  //!< The buffer
                                 ) override;

    //! Handler implementation for saBufferReturnIn
    //!
    //! Ports for receiving back iv/data buffers from downstream decryptors for deallocation
    void saBufferReturnIn_handler(FwIndexType portNum,  //!< The port number
                                  Fw::Buffer& fwBuffer  //!< The buffer
                                  ) override;

    //! Handler implementation for saDecryptIn
    //!
    //! Ports for receiving decrypted data (possibly newly allocated) from downstream decryptors
    void saDecryptIn_handler(FwIndexType portNum,  //!< The port number
                             Fw::Buffer& fwBuffer  //!< The buffer
                             ) override;
};

}  // namespace Ccsds

}  // namespace Svc

#endif
