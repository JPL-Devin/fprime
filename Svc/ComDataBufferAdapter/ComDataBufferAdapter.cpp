// ======================================================================
// \title  ComDataBufferAdapter.cpp
// \brief  cpp file for ComDataBufferAdapter component implementation class
// ======================================================================

#include "Svc/ComDataBufferAdapter/ComDataBufferAdapter.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ComDataBufferAdapter::ComDataBufferAdapter(const char* const compName)
    : ComDataBufferAdapterComponentBase(compName), m_context() {}

ComDataBufferAdapter::~ComDataBufferAdapter() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void ComDataBufferAdapter::configure(const ComCfg::FrameContext& context) {
    this->m_context = context;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ComDataBufferAdapter::bufferIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    // Ownership passes downstream; the buffer comes back on dataReturnIn
    this->dataOut_out(0, fwBuffer, this->m_context);
}

void ComDataBufferAdapter::dataReturnIn_handler(FwIndexType portNum,
                                                Fw::Buffer& data,
                                                const ComCfg::FrameContext& context) {
    this->bufferInReturn_out(0, data);
}

void ComDataBufferAdapter::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Ownership passes to the client; the buffer comes back on bufferOutReturn
    this->bufferOut_out(0, data);
}

void ComDataBufferAdapter::bufferOutReturn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    // The receive context is not tracked across the round trip; deframers ignore it on return
    ComCfg::FrameContext context;
    this->dataReturnOut_out(0, fwBuffer, context);
}

}  // namespace Svc
