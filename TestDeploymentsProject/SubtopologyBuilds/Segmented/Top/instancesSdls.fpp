module Segmented {

    @ Key length in octets read from the key file (AES-256-GCM TEST key, 32 octets 0x40..0x5F, in
    @ test/int/sdls_test_key.bin; equals SdlsCfg.MAX_SDLS_KEY_SIZE).
    constant SDLS_TEST_KEY_SIZE = 32

    @ File-backed key manager for the AES-GCM decryptor: one key for every SA
    @ (SdlsFileKeyManager.fpp documents that the SA index is ignored). The key file is a
    @ TEST key checked into the repository; it provides no security whatsoever.
    instance sdlsKeyManager: Svc.Ccsds.SdlsFileKeyManager base id 0x10030000 \
    {
        phase Fpp.ToCpp.Phases.configComponents """
        FW_ASSERT(state.sdlsKeyFile != nullptr);
        Segmented::sdlsKeyManager.configure(state.sdlsKeyFile, Segmented::SDLS_TEST_KEY_SIZE);
        """
    }

}
