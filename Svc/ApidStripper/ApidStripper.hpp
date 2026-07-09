// ======================================================================
// \title  ApidStripper.hpp
// \brief  hpp file for ApidStripper component implementation class
// ======================================================================

#ifndef Svc_ApidStripper_HPP
#define Svc_ApidStripper_HPP

#include "Svc/ApidStripper/ApidStripperComponentAc.hpp"

namespace Svc {

class ApidStripper final : public ApidStripperComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ApidStripper object
    ApidStripper(const char* const compName  //!< The component name
    );

    //! Destroy ApidStripper object
    ~ApidStripper();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //!
    //! Port receiving APID-prepended buffers
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& fwBuffer) override;

    //! Handler implementation for dataReturnIn
    //!
    //! Port receiving back ownership of buffers emitted on dataOut
    void dataReturnIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& fwBuffer) override;
};

}  // namespace Svc

#endif
