# Configuration override for the SDLS Segmented deployment. Replaces the default
# Svc/Subtopologies/ComCcsdsSdls/ComCcsdsSdlsConfig/ComCcsdsSdlsConfig.fpp in its entirety:
# the override must reproduce ComCcsdsSdlsConfig.BASE_ID and every instance the ComCcsdsSdls
# topologies use (decryptor, encryptor).
module ComCcsdsSdlsConfig {
    # Base ID for the ComCcsdsSdls Subtopology; the SDLS decryption instances are offsets
    # from this base ID. The packet and transfer frame layers are reused from the ComCcsds
    # subtopology and are configured through ComCcsdsConfig.
    constant BASE_ID = 0x06000000
}

module ComCcsdsSdls {
    @ MAC-verifying decryptor, connected to the SdlsSaRouter's SdlsCfg.SaRouterPorts.PLAINTEXT
    @ port, which the default SA map reaches with SA 1. AES-GCM authenticates the 20-octet
    @ per-frame AAD (including the received Segment Header octet) and the frame data field, so
    @ nothing reaches ComCcsds.tcMapReassembler unless the MAC verified. Keyed through the
    @ keyGet port (Segmented.sdlsKeyManager in this deployment).
    instance decryptor: Svc.Ccsds.AesGcmDecryptor base id ComCcsdsSdlsConfig.BASE_ID + 0x02000

    @ Downlink encryptor. Left clear-text: this deployment exercises the authenticated uplink
    @ only, and the clear-text encryptor keeps telemetry decodable by the stock GDS deframer
    @ (after the 2-octet SA index CcsdsSdlsFramer prepends). It raises NullCipherInUse on
    @ every TM frame; the uplink signature of a misconfiguration is decryptor.NullCipherInUse.
    @
    @ WARNING: Svc.Ccsds.ClearTextEncryptor provides NO security: no confidentiality, no
    @ integrity, and no authentication.
    instance encryptor: Svc.Ccsds.ClearTextEncryptor base id ComCcsdsSdlsConfig.BASE_ID + 0x04000
}
