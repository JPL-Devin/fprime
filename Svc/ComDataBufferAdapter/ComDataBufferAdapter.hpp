// ======================================================================
// \title  ComDataBufferAdapter.hpp
// \brief  hpp file for ComDataBufferAdapter component implementation class
// ======================================================================

#ifndef Svc_ComDataBufferAdapter_HPP
#define Svc_ComDataBufferAdapter_HPP

#include "Svc/ComDataBufferAdapter/ComDataBufferAdapterComponentAc.hpp"

namespace Svc {

class ComDataBufferAdapter final : public ComDataBufferAdapterComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ComDataBufferAdapter object
    explicit ComDataBufferAdapter(const char* const compName  //!< The component name
    );

    //! Destroy ComDataBufferAdapter object
    ~ComDataBufferAdapter();

    ComDataBufferAdapter(const ComDataBufferAdapter&) = delete;
    ComDataBufferAdapter& operator=(const ComDataBufferAdapter&) = delete;

    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Set the frame context sent with every buffer emitted on dataOut
    //!
    //! Defaults to a default-constructed ComCfg::FrameContext. Framers that read
    //! the context (e.g. CCSDS framers using apid or vcId) need it configured.
    void configure(const ComCfg::FrameContext& context  //!< The frame context
    );

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for bufferIn
    //!
    //! Port for receiving buffers
    void bufferIn_handler(FwIndexType portNum,  //!< The port number
                          Fw::Buffer& fwBuffer  //!< The buffer
                          ) override;

    //! Handler implementation for bufferOutReturn
    //!
    //! Port for receiving buffers sent on bufferOut and then returned
    void bufferOutReturn_handler(FwIndexType portNum,  //!< The port number
                                 Fw::Buffer& fwBuffer  //!< The buffer
                                 ) override;

    //! Handler implementation for dataIn
    //!
    //! Port for receiving deframed data, forwarded on bufferOut
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& data,
                        const ComCfg::FrameContext& context) override;

    //! Handler implementation for dataReturnIn
    //!
    //! Port for receiving back ownership of buffers sent on dataOut
    void dataReturnIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& data,
                              const ComCfg::FrameContext& context) override;

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    //! Frame context sent with every buffer on dataOut
    ComCfg::FrameContext m_context;
};

}  // namespace Svc

#endif
