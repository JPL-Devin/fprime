# ======================================================================
# FPP file for configuration of SDLS (Space Data Link Security) components
# ======================================================================

module SdlsCfg {

    @ Number of downstream decryptor ports on the SdlsSaRouter
    constant SaRouterPortCount = 4

    @ Number of entries in the SA-to-port routing map
    constant SaRouterMapEntryCount = 4

    @ Maximum number of decrypted data buffers outstanding (sent downstream, not yet returned)
    constant SaRouterMaxOutstandingBuffers = 4

}
