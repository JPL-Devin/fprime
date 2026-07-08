// ======================================================================
// \title  SdlsSaRouter.cpp
// \author lestarch-autobot
// \brief  cpp file for SdlsSaRouter component implementation class
// ======================================================================

#include "Svc/Ccsds/SdlsSaRouter/SdlsSaRouter.hpp"

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

SdlsSaRouter ::SdlsSaRouter(const char* const compName) : SdlsSaRouterComponentBase(compName) {}

SdlsSaRouter ::~SdlsSaRouter() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Svc::Ccsds::SdlsStatus SdlsSaRouter ::decryptIn_handler(FwIndexType portNum,
                                                        U16 securityAssociationIndex,
                                                        Fw::Buffer& data) {
    return Svc::Ccsds::SdlsStatus::UNKNOWN_SA;  // TODO: implement routing
}

void SdlsSaRouter ::decryptReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    // TODO
}

void SdlsSaRouter ::saBufferReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    // TODO
}

void SdlsSaRouter ::saDecryptIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    // TODO
}

}  // namespace Ccsds

}  // namespace Svc
