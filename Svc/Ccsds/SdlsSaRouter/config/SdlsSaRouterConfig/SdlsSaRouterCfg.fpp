# ======================================================================
# SdlsSaRouterCfg.fpp
# Compile-time configuration for the SdlsSaRouter component
# ======================================================================

module SdlsCfg {

    @ Number of downstream decryptor ports on the SdlsSaRouter
    constant SaRouterPortCount = 2

    @ Number of entries in the SA-to-port routing map
    constant SaRouterMapEntryCount = 2

    @ Maximum number of decrypted data buffers outstanding (sent downstream, not yet returned)
    constant SaRouterMaxOutstandingBuffers = 4

    @ Base security association index: the SA handled by the default (port 0) decryptor
    constant SaRouterBaseSa = 0

    @ Compile-time map from security association index to downstream port index. Projects
    @ may define sparse or non-linear SA ranges that map down to a compact, linear port
    @ array. Port indices must be in [0, SaRouterPortCount).
    @
    @ This default configuration maps the base SA to port 0; the second entry maps the
    @ next SA to port 1, which standard topologies may leave unconnected.
    array SaMap = [SaRouterMapEntryCount] Svc.Ccsds.SaMapEntry default [
        { securityAssociationIndex = SaRouterBaseSa, portIndex = 0 },
        { securityAssociationIndex = SaRouterBaseSa + 1, portIndex = 1 }
    ]

}
