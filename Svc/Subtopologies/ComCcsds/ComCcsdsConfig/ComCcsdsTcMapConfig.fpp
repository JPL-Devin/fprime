module ComCcsds {

    # NOTE: Projects override this configuration file to change the accepted (Virtual Channel,
    # MAP ID) pairs of the segmented TC uplink (ComCcsds.TcMapExtraction / *Segmented topologies).
    # The table is a C++ array in phase code, NOT an FPP array constant: fpp-to-cpp emits C++ for
    # scalar, string and enum constants only, so an FPP array constant cannot be read from C++.
    # The entry count must equal TcMapCfg.MapChannelCount (TcMapCfg.fpp), every VCID and MAP ID
    # must be <= 63 and pairs must be distinct; TcMapReassembler::configure() asserts this at init.
    # A MAP is a channel within one Virtual Channel: the same MAP ID on two VCIDs is two
    # independent reassembly channels, never shared state.
    instance tcMapReassembler: Svc.Ccsds.TcMapReassembler base id ComCcsdsConfig.BASE_ID + 0x0D000 \
    {
        phase Fpp.ToCpp.Phases.configObjects """
        // Accepted (VCID, MAP ID) pairs, one per reassembly channel (CCSDS 232.0-B-4 2.1.3, 4.1.3.2.2.2,
        // 4.4.3.3). Default: MAP 0 of Virtual Channel 1, the fprime-gds TC framer default (--vcid 1).
        const Svc::Ccsds::TcMapReassembler::MapKey channels[TcMapCfg::MapChannelCount] = {{1, 0}};
        """

        phase Fpp.ToCpp.Phases.configComponents """
        ComCcsds::tcMapReassembler.configure(
            ConfigObjects::ComCcsds_tcMapReassembler::channels,
            TcMapCfg::MapChannelCount
        );
        """
    }

    # Example: a project with TcMapCfg.MapChannelCount = 2 overrides both TcMapCfg.fpp and this
    # file, e.g. `... channels[TcMapCfg::MapChannelCount] = {{1, 0}, {1, 5}};` (two MAPs of VC 1)
    # or `{{1, 0}, {2, 0}}` (MAP 0 of VC 1 and of VC 2).
}
