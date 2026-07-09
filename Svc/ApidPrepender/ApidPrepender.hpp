// ======================================================================
// \title  ApidPrepender.hpp
// \brief  hpp file for ApidPrepender component implementation class
// ======================================================================

#ifndef Svc_ApidPrepender_HPP
#define Svc_ApidPrepender_HPP

#include "Svc/ApidPrepender/ApidPrependerComponentAc.hpp"

namespace Svc {

class ApidPrepender final : public ApidPrependerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ApidPrepender object
    ApidPrepender(const char* const compName  //!< The component name
    );

    //! Destroy ApidPrepender object
    ~ApidPrepender();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //!
    //! Port receiving data with its APID
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& fwBuffer,
                        const ComCfg::Apid& apid) override;

    //! Handler implementation for dataOutReturn
    //!
    //! Port receiving back ownership of buffers emitted on dataOut
    void dataOutReturn_handler(FwIndexType portNum,  //!< The port number
                               Fw::Buffer& fwBuffer) override;
};

}  // namespace Svc

#endif
