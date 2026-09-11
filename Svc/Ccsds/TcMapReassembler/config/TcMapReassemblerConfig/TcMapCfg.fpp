# ======================================================================
# TcMapCfg.fpp
# Compile-time configuration for the TcMapReassembler component
# ======================================================================

@ Compile-time bounds for Svc.Ccsds.TcMapReassembler and its dedicated buffer pool
module TcMapCfg {
    @ Number of MAP channels the reassembler tracks (one reassembly slot each). 1..64.
    constant MapChannelCount = 1

    @ Largest reassembled Space Packet accepted, in octets. 7..65542 (CCSDS 133.0-B-2 4.1.2.2).
    constant MaxPacketSize = 4096

    @ Completed packets that may be outstanding downstream (delivered on dataOut, not yet returned on dataReturnIn)
    constant MaxPacketsInFlight = 4

    @ Buffers in the dedicated pool: one per MAP in progress + completed packets in flight
    constant PoolBufferCount = MapChannelCount + MaxPacketsInFlight

    @ Backing memory of the dedicated pool
    constant PoolBytes = PoolBufferCount * MaxPacketSize

    @ Svc.BufferManager instance identifier for the dedicated pool (distinct from ComCcsdsConfig.BuffMgr.commsBuffMgrId = 200)
    constant PoolManagerId = 201

    @ Space Packet primary header size and length bounds (CCSDS 133.0-B-2 4.1.2.2, 4.1.3.5)
    constant SpacePacketHeaderSize = 6
    constant SpacePacketMinSize    = 7
    constant SpacePacketMaxSize    = 65542
}
