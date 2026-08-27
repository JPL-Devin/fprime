// ======================================================================
// \title  CcsdsSdlsFramer.cpp
// \author devin
// \brief  cpp file for CcsdsSdlsFramer component implementation class
// ======================================================================

#include "Svc/Ccsds/CcsdsSdlsFramer/CcsdsSdlsFramer.hpp"

#include <Fw/Prm/ParamValid.hpp>

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

CcsdsSdlsFramer ::CcsdsSdlsFramer(const char* const compName) : CcsdsSdlsFramerComponentBase(compName) {}

CcsdsSdlsFramer ::~CcsdsSdlsFramer() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void CcsdsSdlsFramer ::bufferReturnIn_handler(FwIndexType portNum,
                                              Fw::Buffer& data,
                                              const ComCfg::FrameContext& context) {
    // The encryption helper has returned the original data buffer: send it back upstream
    this->dataReturnOut_out(0, data, context);
}

void CcsdsSdlsFramer ::comStatusIn_handler(FwIndexType portNum, Fw::Success& condition) {
    if (this->isConnected_comStatusOut_OutputPort(portNum)) {
        this->comStatusOut_out(portNum, condition);
    }
}

void CcsdsSdlsFramer ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Determine the security association index: use the context's value when set, otherwise the SA_INDEX parameter
    const U16 unsetSaIndex = static_cast<U16>(ComCfg::SaIndexUnset);
    U16 saIndex = context.get_saIndex();
    if (saIndex == unsetSaIndex) {
        Fw::ParamValid valid = Fw::ParamValid::INVALID;
        saIndex = this->paramGet_SA_INDEX(valid);
        FW_ASSERT(FW_PARAM_OK(valid), static_cast<FwAssertArgType>(valid.e));
    }

    // Copy context and record the security association index used for encryption
    ComCfg::FrameContext newContext = context;
    newContext.set_saIndex(saIndex);

    if (context.get_zeroCopyFrame()) {
        // Record the zone identity so the in-place encryption contract can be checked on encryptIn
        this->m_zeroCopyData = data.getData();
        this->m_zeroCopySize = data.getSize();
    } else {
        this->m_zeroCopyData = nullptr;
        this->m_zeroCopySize = 0;
    }

    this->encryptOut_out(0, saIndex, data, newContext);
}

void CcsdsSdlsFramer ::dataReturnIn_handler(FwIndexType portNum,
                                            Fw::Buffer& data,
                                            const ComCfg::FrameContext& context) {
    if ((this->m_zeroCopyFrame != nullptr) && (data.getData() == this->m_zeroCopyFrame)) {
        // The frame is the upstream-owned zero-copy zone buffer: forward ownership back upstream
        this->m_zeroCopyFrame = nullptr;
        this->dataReturnOut_out(0, data, context);
    } else {
        // dataReturnIn is a self-allocated frame buffer coming back from the dataOut port
        this->bufferDeallocate_out(0, data);
    }
}

void CcsdsSdlsFramer ::encryptIn_handler(FwIndexType portNum,
                                         const Svc::Ccsds::SdlsStatus& status,
                                         Fw::Buffer& data,
                                         const ComCfg::FrameContext& context) {
    if (status != Svc::Ccsds::SdlsStatus::SUCCESS) {
        this->log_WARNING_HI_EncryptionFailed(status);
        // Drop the frame: return ownership of the buffer to the encryption subsystem
        this->encryptReturnOut_out(0, data, context);
        this->sendComStatusOnDrop();
        return;
    }

    // In-place zero-copy path: the encryptor returned the same buffer (encrypted in place,
    // ciphertext length equal to plaintext length) and the buffer carries headroom for the
    // SA index. Ownership stays with this framer (no encryptReturnOut): the buffer flows
    // downstream and is returned upstream when the frame comes back on dataReturnIn
    if (context.get_zeroCopyFrame() && (data.getData() == this->m_zeroCopyData) &&
        (data.getSize() == this->m_zeroCopySize) && (data.getOffset() >= sizeof(U16))) {
        data.advance(-static_cast<FwSignedSizeType>(sizeof(U16)));
        auto frameSerializer = data.getSerializer();
        Fw::SerializeStatus serializeStatus = frameSerializer.serializeFrom(context.get_saIndex());
        FW_ASSERT(serializeStatus == Fw::FW_SERIALIZE_OK, serializeStatus);

        // Only one zero-copy zone can be in flight: the packer holds pool ownership until return
        FW_ASSERT(this->m_zeroCopyFrame == nullptr);
        this->m_zeroCopyFrame = data.getData();
        this->m_zeroCopyData = nullptr;
        this->m_zeroCopySize = 0;
        this->dataOut_out(0, data, context);
        return;
    }

    // The in-place contract could not be met (new/resized buffer or no headroom):
    // fall back to allocate-and-copy
    this->frame_allocate_and_copy(data, context);
}

void CcsdsSdlsFramer ::frame_allocate_and_copy(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    FW_ASSERT(data.getSize() <= std::numeric_limits<Fw::Buffer::SizeType>::max() - sizeof(U16),
              static_cast<FwAssertArgType>(data.getSize()));
    const Fw::Buffer::SizeType frameSize = static_cast<Fw::Buffer::SizeType>(data.getSize() + sizeof(U16));

    // Allocate the frame buffer used to prepend the security association index to the encrypted data
    Fw::Buffer frameBuffer = this->bufferAllocate_out(0, frameSize);
    if ((!frameBuffer.isValid()) || (frameBuffer.getSize() < frameSize)) {
        this->log_WARNING_HI_BufferAllocationFailed(frameSize);
        // Drop the frame: return the undersized allocation (when valid) and the encrypted data buffer
        if (frameBuffer.isValid()) {
            this->bufferDeallocate_out(0, frameBuffer);
        }
        this->encryptReturnOut_out(0, data, context);
        this->sendComStatusOnDrop();
        return;
    }

    auto frameSerializer = frameBuffer.getSerializer();
    Fw::SerializeStatus serializeStatus = frameSerializer.serializeFrom(context.get_saIndex());
    FW_ASSERT(serializeStatus == Fw::FW_SERIALIZE_OK, serializeStatus);
    serializeStatus = frameSerializer.serializeFrom(data.getData(), data.getSize(), Fw::Serialization::OMIT_LENGTH);
    FW_ASSERT(serializeStatus == Fw::FW_SERIALIZE_OK, serializeStatus);

    // Trim to actual frame size in case the allocator returned a larger buffer
    frameBuffer.setSize(frameSize);

    // The allocated frame carries no headroom/trailer reserve: downstream must copy
    ComCfg::FrameContext frameContext = context;
    frameContext.set_zeroCopyFrame(false);

    // Return ownership of the encrypted data buffer to the encryption helper, then send the frame
    this->encryptReturnOut_out(0, data, context);
    this->dataOut_out(0, frameBuffer, frameContext);
}

// ----------------------------------------------------------------------
// Private helper implementations
// ----------------------------------------------------------------------

void CcsdsSdlsFramer ::sendComStatusOnDrop() {
    // Report ready-for-more on a dropped frame so a ComQueue-driven downlink does not stall
    if (this->isConnected_comStatusOut_OutputPort(0)) {
        Fw::Success status = Fw::Success::SUCCESS;
        this->comStatusOut_out(0, status);
    }
}

}  // namespace Ccsds

}  // namespace Svc
