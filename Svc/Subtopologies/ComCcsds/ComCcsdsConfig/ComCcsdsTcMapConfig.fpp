module ComCcsds {

    # NOTE: Projects override this configuration file to change the accepted MAP IDs of the
    # segmented TC uplink (ComCcsds.TcMapExtraction / *Segmented topologies). The table is a
    # C++ array in phase code, NOT an FPP array constant: fpp-to-cpp emits C++ for scalar,
    # string and enum constants only, so an FPP array constant cannot be read from C++.
    # The entry count must equal TcMapCfg.MapChannelCount (TcMapCfg.fpp), every entry must be
    # <= 63 and entries must be distinct; TcMapReassembler::configure() asserts this at init.
    instance tcMapReassembler: Svc.Ccsds.TcMapReassembler base id ComCcsdsConfig.BASE_ID + 0x0D000 \
    {
        phase Fpp.ToCpp.Phases.configObjects """
        // Accepted MAP IDs, one per reassembly channel (CCSDS 232.0-B-4 4.1.3.2.2.2, 4.4.3.3). Default: MAP 0 only.
        const U8 mapIds[TcMapCfg::MapChannelCount] = {0};
        """

        phase Fpp.ToCpp.Phases.configComponents """
        ComCcsds::tcMapReassembler.configure(
            ConfigObjects::ComCcsds_tcMapReassembler::mapIds,
            TcMapCfg::MapChannelCount
        );
        """
    }

    # Example: a project with TcMapCfg.MapChannelCount = 2 overrides both TcMapCfg.fpp and this
    # file, e.g. `const U8 mapIds[TcMapCfg::MapChannelCount] = {0, 5};`
}
