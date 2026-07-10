# SDLS decryption layer: the SDLS deframer (SA extraction), the SA router,
# and the default decryptor.
#
# Instance names are fully qualified so that this file may be included from
# topologies defined in other modules.

instance ComCcsdsSdls.sdlsDeframer
instance ComCcsdsSdls.saRouter
instance ComCcsdsSdls.decryptor

connections Decryption {
    # CcsdsSdlsDeframer <-> SdlsSaRouter (decryption requests and returns)
    ComCcsdsSdls.sdlsDeframer.decryptOut       -> ComCcsdsSdls.saRouter.decryptIn
    ComCcsdsSdls.saRouter.decryptOut           -> ComCcsdsSdls.sdlsDeframer.decryptIn
    ComCcsdsSdls.sdlsDeframer.decryptReturnOut -> ComCcsdsSdls.saRouter.decryptReturnIn
    ComCcsdsSdls.saRouter.bufferReturnOut      -> ComCcsdsSdls.sdlsDeframer.bufferReturnIn

    # SdlsSaRouter <-> default decryptor
    ComCcsdsSdls.saRouter.saDecryptOut[SdlsCfg.SaRouterPorts.PLAINTEXT_DECRYPTION]       -> ComCcsdsSdls.decryptor.decryptIn
    ComCcsdsSdls.decryptor.decryptOut           -> ComCcsdsSdls.saRouter.saDecryptIn[SdlsCfg.SaRouterPorts.PLAINTEXT_DECRYPTION]
    ComCcsdsSdls.saRouter.saDecryptReturnOut[SdlsCfg.SaRouterPorts.PLAINTEXT_DECRYPTION] -> ComCcsdsSdls.decryptor.decryptReturnIn
    ComCcsdsSdls.decryptor.bufferReturnOut      -> ComCcsdsSdls.saRouter.saBufferReturnIn[SdlsCfg.SaRouterPorts.PLAINTEXT_DECRYPTION]
}
