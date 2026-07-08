# ======================================================================
# FPP file for configuration of SDLS (Space Data Link Security) components
# ======================================================================

module SdlsCfg {

    @ Number of downstream decryptor ports on the SdlsSaRouter
    constant SaRouterPortCount = 4

    @ Number of entries in the SA-to-port routing map
    constant SaRouterMapEntryCount = 4

    @ Compile-time map from security association index to downstream port index.
    @ Projects may define sparse or non-linear SA ranges mapping to a compact port array.
    array SaMap = [SaRouterMapEntryCount] Svc.Ccsds.SaMapEntry default [
        { securityAssociationIndex = 0, portIndex = 0 },
        { securityAssociationIndex = 1, portIndex = 1 },
        { securityAssociationIndex = 2, portIndex = 2 },
        { securityAssociationIndex = 3, portIndex = 3 }
    ]

}
