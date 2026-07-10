# Connections composing the ComCcsds SpacePacketFraming packet layer and the
# ComCcsds TmTcFraming transfer frame layer with the SdlsDecryption layer
# inserted in between on the uplink path.
#
# Instance names are fully qualified so that this file may be included from
# topologies defined in other modules.

connections Downlink {
    # SpacePacketFraming <-> TmTcFraming (downlink is not encrypted)
    ComCcsds.aggregator.dataOut   -> ComCcsds.framer.dataIn
    ComCcsds.framer.dataReturnOut -> ComCcsds.aggregator.dataReturnIn

    # ComStatus
    ComCcsds.framer.comStatusOut -> ComCcsds.aggregator.comStatusIn
    # (Outgoing) TmTcFraming <-> ComInterface connections shall be established by the user
}

connections Uplink {
    # (Incoming) ComInterface <-> TmTcFraming connections shall be established by the user
    # TmTcFraming buffer allocations
    ComCcsds.frameAccumulator.bufferDeallocate -> ComCcsds.commsBufferManager.bufferSendIn
    ComCcsds.frameAccumulator.bufferAllocate   -> ComCcsds.commsBufferManager.bufferGetCallee

    # TmTcFraming <-> SdlsDecryption (SDLS decryption step)
    ComCcsds.tcDeframer.dataOut             -> ComCcsdsSdls.sdlsDeframer.dataIn
    ComCcsdsSdls.sdlsDeframer.dataReturnOut -> ComCcsds.tcDeframer.dataReturnIn

    # SdlsDecryption <-> SpacePacketFraming
    ComCcsdsSdls.sdlsDeframer.dataOut          -> ComCcsds.spacePacketDeframer.dataIn
    ComCcsds.spacePacketDeframer.dataReturnOut -> ComCcsdsSdls.sdlsDeframer.dataReturnIn
}
