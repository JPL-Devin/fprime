module ComCcsds {

    # ComPacket Queue enum for queue types
    enum Ports_ComPacketQueue : U8 {
        EVENTS,
        TELEMETRY 
    }

    enum Ports_ComBufferQueue : U8 {
        FILE
    }

    # ----------------------------------------------------------------------
    # Active Components
    # ----------------------------------------------------------------------
    instance comQueue: Svc.ComQueue base id ComCcsdsConfig.BASE_ID + 0x00000 \
        queue size ComCcsdsConfig.QueueSizes.comQueue \
        stack size ComCcsdsConfig.StackSizes.comQueue \
        priority ComCcsdsConfig.Priorities.comQueue \
        cpu ComCcsdsConfig.CpuAffinities.comQueue \
    {
        phase Fpp.ToCpp.Phases.configComponents """
        using namespace ComCcsds;
        Svc::ComQueue::QueueConfigurationTable configurationTable;

        // Events (highest-priority)
        configurationTable.entries[Ports_ComPacketQueue::EVENTS].depth = ComCcsdsConfig::QueueDepths::events;
        configurationTable.entries[Ports_ComPacketQueue::EVENTS].priority = ComCcsdsConfig::QueuePriorities::events;

        // Telemetry
        configurationTable.entries[Ports_ComPacketQueue::TELEMETRY].depth = ComCcsdsConfig::QueueDepths::tlm;
        configurationTable.entries[Ports_ComPacketQueue::TELEMETRY].priority = ComCcsdsConfig::QueuePriorities::tlm;

        // File Downlink Queue (buffer queue using NUM_CONSTANTS offset)
        configurationTable.entries[Ports_ComPacketQueue::NUM_CONSTANTS + Ports_ComBufferQueue::FILE].depth = ComCcsdsConfig::QueueDepths::file;
        configurationTable.entries[Ports_ComPacketQueue::NUM_CONSTANTS + Ports_ComBufferQueue::FILE].priority = ComCcsdsConfig::QueuePriorities::file;

        // Allocation identifier is 0 as the MallocAllocator discards it
        ComCcsds::comQueue.configure(configurationTable, 0, ComCcsds::Allocation::memAllocator);
        """
        phase Fpp.ToCpp.Phases.tearDownComponents """
        ComCcsds::comQueue.cleanup();
        """
    }

    # ----------------------------------------------------------------------
    # Passive Components
    # ----------------------------------------------------------------------
    instance frameAccumulator: Svc.FrameAccumulator base id ComCcsdsConfig.BASE_ID + 0x01000 \ 
    {

        phase Fpp.ToCpp.Phases.configObjects """
        Svc::FrameDetectors::CcsdsTcFrameDetector frameDetector;
        """
        phase Fpp.ToCpp.Phases.configComponents """
        ComCcsds::frameAccumulator.configure(
            ConfigObjects::ComCcsds_frameAccumulator::frameDetector,
            1,
            ComCcsds::Allocation::memAllocator,
            ComCcsdsConfig::BuffMgr::frameAccumulatorSize
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        ComCcsds::frameAccumulator.cleanup();
        """
    }

    instance commsBufferManager: Svc.BufferManager base id ComCcsdsConfig.BASE_ID + 0x02000 \
    {
        phase Fpp.ToCpp.Phases.configObjects """
        Svc::BufferManager::BufferBins bins;
        """

        phase Fpp.ToCpp.Phases.configComponents """
        memset(&ConfigObjects::ComCcsds_commsBufferManager::bins, 0, sizeof(ConfigObjects::ComCcsds_commsBufferManager::bins));
        ConfigObjects::ComCcsds_commsBufferManager::bins.bins[0].bufferSize = ComCcsdsConfig::BuffMgr::commsBuffSize;
        ConfigObjects::ComCcsds_commsBufferManager::bins.bins[0].numBuffers = ComCcsdsConfig::BuffMgr::commsBuffCount;
        ConfigObjects::ComCcsds_commsBufferManager::bins.bins[1].bufferSize = ComCcsdsConfig::BuffMgr::commsFileBuffSize;
        ConfigObjects::ComCcsds_commsBufferManager::bins.bins[1].numBuffers = ComCcsdsConfig::BuffMgr::commsFileBuffCount;
        ComCcsds::commsBufferManager.setup(
            ComCcsdsConfig::BuffMgr::commsBuffMgrId,
            0,
            ComCcsds::Allocation::memAllocator,
            ConfigObjects::ComCcsds_commsBufferManager::bins
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        ComCcsds::commsBufferManager.cleanup();
        """
    }

    # NOTE: the 'fprimeRouter' instance is defined in ComCcsdsConfig/ComCcsdsRouterConfig.fpp so that
    # projects may swap the router implementation via configuration overrides

    instance tcDeframer: Svc.Ccsds.TcDeframer base id ComCcsdsConfig.BASE_ID + 0x04000

    instance spacePacketDeframer: Svc.Ccsds.SpacePacketDeframer base id ComCcsdsConfig.BASE_ID + 0x05000

    instance aggregator: Svc.ComAggregator base id ComCcsdsConfig.BASE_ID + 0x06000 \
        queue size ComCcsdsConfig.QueueSizes.aggregator \
        stack size ComCcsdsConfig.StackSizes.aggregator \
        priority ComCcsdsConfig.Priorities.aggregator \
        cpu ComCcsdsConfig.CpuAffinities.aggregator \
    {
        phase Fpp.ToCpp.Phases.configComponents """
        ComCcsds::aggregator.configure(ComCcsdsConfig::Aggregator::enablePacketSpanning);
        """
    }

    # NOTE: name 'framer' is used for the framer that connects to the Com Adapter Interface for better subtopology interoperability
    instance framer: Svc.Ccsds.TmFramer base id ComCcsdsConfig.BASE_ID + 0x07000

    instance spacePacketFramer: Svc.Ccsds.SpacePacketFramer base id ComCcsdsConfig.BASE_ID + 0x08000

    instance apidManager: Svc.Ccsds.ApidManager base id ComCcsdsConfig.BASE_ID + 0x09000

    instance comStub: Svc.ComStub base id ComCcsdsConfig.BASE_ID + 0x0A000

    # ----------------------------------------------------------------------
    # Segmented-uplink instances (used only by the *Segmented topologies below)
    # ----------------------------------------------------------------------

    @ TC deframer in Segment Header mode (CCSDS 232.0-B-4 4.1.3.2.2). Distinct from
    @ 'tcDeframer' so that Segment Header processing is enabled by importing a Segmented
    @ topology and by nothing else (single switch).
    instance tcDeframerSeg: Svc.Ccsds.TcDeframer base id ComCcsdsConfig.BASE_ID + 0x0B000 \
    {
        phase Fpp.ToCpp.Phases.configComponents """
        ComCcsds::tcDeframerSeg.configureSegmentHeader(true);
        """
    }

    @ Dedicated buffer pool for reassembled Space Packets: one bin,
    @ TcMapCfg.PoolBufferCount buffers of TcMapCfg.MaxPacketSize octets. Never shared
    @ with commsBufferManager (Svc.BufferManager allocates first-fit across bins).
    instance tcPacketBufferManager: Svc.BufferManager base id ComCcsdsConfig.BASE_ID + 0x0C000 \
    {
        phase Fpp.ToCpp.Phases.configObjects """
        Svc::BufferManager::BufferBins bins;
        // fpp constants are anonymous enums: bind each to a named constexpr, then assert on the names
        // (inline static_cast operands fail cmake/flags.cmake -Wconversion -Werror)
        constexpr U16        kPoolManagerId      = TcMapCfg::PoolManagerId;
        constexpr U16        kCommsBuffMgrId     = ComCcsdsConfig::BuffMgr::commsBuffMgrId;
        constexpr FwSizeType kPoolBufferCount    = TcMapCfg::PoolBufferCount;
        constexpr FwSizeType kMapChannelCount    = TcMapCfg::MapChannelCount;
        constexpr FwSizeType kMaxPacketsInFlight = TcMapCfg::MaxPacketsInFlight;
        constexpr U64        kPoolBytes          = TcMapCfg::PoolBytes;
        constexpr U64        kMaxPacketSize      = TcMapCfg::MaxPacketSize;
        static_assert(kPoolManagerId != kCommsBuffMgrId,
                      "dedicated TC packet pool must not reuse the comms BufferManager id");
        static_assert(kPoolBufferCount == kMapChannelCount + kMaxPacketsInFlight,
                      "TC packet pool must hold one buffer per MAP plus MaxPacketsInFlight");
        static_assert(kPoolBytes == static_cast<U64>(kPoolBufferCount) * kMaxPacketSize,
                      "TC packet pool backing memory arithmetic");
        """

        phase Fpp.ToCpp.Phases.configComponents """
        memset(&ConfigObjects::ComCcsds_tcPacketBufferManager::bins, 0, sizeof(ConfigObjects::ComCcsds_tcPacketBufferManager::bins));
        ConfigObjects::ComCcsds_tcPacketBufferManager::bins.bins[0].bufferSize = TcMapCfg::MaxPacketSize;
        ConfigObjects::ComCcsds_tcPacketBufferManager::bins.bins[0].numBuffers = TcMapCfg::PoolBufferCount;
        ComCcsds::tcPacketBufferManager.setup(
            TcMapCfg::PoolManagerId,
            0,
            ComCcsds::Allocation::memAllocator,
            ConfigObjects::ComCcsds_tcPacketBufferManager::bins
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        ComCcsds::tcPacketBufferManager.cleanup();
        """
    }

    # NOTE: the 'tcMapReassembler' instance is defined in ComCcsdsConfig/ComCcsdsTcMapConfig.fpp so that
    # projects may change the accepted MAP ID table via configuration overrides

    # This subtopology boxes the Space Packet packet layer: router, ComQueue, space packet
    # framer/deframer, APID manager, aggregator, and comms buffer manager.
    topology SpacePacketFraming {
        # Usage Note:
        #
        # When importing this subtopology, users shall establish 5 port connections with a downstream
        # framing layer or a component implementing the Svc.Com (Svc/Interfaces/Com.fpp) interface:
        #
        # 1) Outputs:
        #     - ComCcsds.SpacePacketFraming.dataOut       -> [downstream].dataIn
        #     - ComCcsds.SpacePacketFraming.dataReturnOut -> [downstream].dataReturnIn
        # 2) Inputs:
        #     - [downstream].dataReturnOut -> ComCcsds.SpacePacketFraming.dataReturnIn
        #     - [downstream].comStatusOut  -> ComCcsds.SpacePacketFraming.comStatusIn
        #     - [downstream].dataOut       -> ComCcsds.SpacePacketFraming.dataIn

        # Active Components
        instance comQueue

        # Passive Components
        instance commsBufferManager
        instance fprimeRouter
        instance spacePacketDeframer
        instance spacePacketFramer
        instance apidManager
        instance aggregator

        connections Downlink {
            # ComQueue <-> SpacePacketFramer
            comQueue.dataOut                -> spacePacketFramer.dataIn
            spacePacketFramer.dataReturnOut -> comQueue.dataReturnIn
            # SpacePacketFramer buffer and APID management
            spacePacketFramer.bufferAllocate   -> commsBufferManager.bufferGetCallee
            spacePacketFramer.bufferDeallocate -> commsBufferManager.bufferSendIn
            spacePacketFramer.getApidSeqCount  -> apidManager.getApidSeqCountIn
            # SpacePacketFramer <-> ComAggregator
            spacePacketFramer.dataOut -> aggregator.dataIn
            aggregator.dataReturnOut  -> spacePacketFramer.dataReturnIn

            # ComStatus
            aggregator.comStatusOut        -> spacePacketFramer.comStatusIn
            spacePacketFramer.comStatusOut -> comQueue.comStatusIn
            # (Outgoing) Aggregator <-> downstream connections shall be established by the user
        }

        connections Uplink {
            # (Incoming) downstream <-> SpacePacketDeframer connections shall be established by the user
            # SpacePacketDeframer APID validation
            spacePacketDeframer.validateApidSeqCount -> apidManager.validateApidSeqCountIn
            # SpacePacketDeframer <-> Router
            spacePacketDeframer.dataOut -> fprimeRouter.dataIn
            fprimeRouter.dataReturnOut  -> spacePacketDeframer.dataReturnIn
        }

        # ----------------------------------------------------------------------
        # Topology ports (open framing boundary)
        # ----------------------------------------------------------------------

        @ Output port sending aggregated space packets to the downstream framing layer
        port dataOut       = aggregator.dataOut

        @ Input port receiving back ownership of downlinked buffers from the downstream framing layer
        port dataReturnIn  = aggregator.dataReturnIn

        @ Input port receiving com status from the downstream framing layer
        port comStatusIn   = aggregator.comStatusIn

        @ Input port receiving space packets from the downstream framing layer for deframing
        port dataIn        = spacePacketDeframer.dataIn

        @ Output port returning ownership of uplinked buffers to the downstream framing layer
        port dataReturnOut = spacePacketDeframer.dataReturnOut

        # Buffer management boundary
        @ Input port for requesting (allocating) a new Fw::Buffer from the comms buffer pool
        port bufferGetCallee = commsBufferManager.bufferGetCallee

        @ Input port for deallocating Fw::Buffers back into the comms buffer pool
        port bufferSendIn    = commsBufferManager.bufferSendIn
    } # end SpacePacketFraming

    # This subtopology uses SpacePacketFraming with a ComStub component for Com Interface,
    # providing a space-packet-only stack with no transfer frame layer.
    topology SpacePacket {
        import SpacePacketFraming

        instance comStub

        connections SpacePacketComStub {
            # SpacePacketFraming <-> ComStub (Downlink)
            SpacePacketFraming.dataOut -> comStub.dataIn
            comStub.dataReturnOut      -> SpacePacketFraming.dataReturnIn
            comStub.comStatusOut       -> SpacePacketFraming.comStatusIn

            # ComStub <-> SpacePacketFraming (Uplink)
            comStub.dataOut -> SpacePacketFraming.dataIn
            SpacePacketFraming.dataReturnOut -> comStub.dataReturnIn
        }

        # ----------------------------------------------------------------------
        # Topology ports
        # ----------------------------------------------------------------------

        # Command routing
        @ Output port sending routed command packets to the command dispatcher
        port commandOut         = fprimeRouter.commandOut

        @ Input port receiving command response messages back into the router
        port cmdResponseIn      = fprimeRouter.cmdResponseIn

        @ Output port sending uplinked file packets to the file handling stack
        port fileUplinkOut          = fprimeRouter.fileOut

        @ Input port receiving back buffer ownership from the file handling stack
        port fileUplinkReturnIn = fprimeRouter.fileBufferReturnIn

        # Telemetry/events/file queuing (array ports - index at connection site)
        @ Input port array for queueing Fw::ComBuffers
        port comPacketQueueIn = comQueue.comPacketQueueIn

        @ Input port array for queueing Fw::Buffers
        port bufferQueueIn    = comQueue.bufferQueueIn

        @ Output port array returning ownership of Fw::Buffers to their original sender after dequeuing
        port bufferReturnOut  = comQueue.bufferReturnOut

        # ComDriver interface (via ComStub)
        @ Input port receiving data read from the ByteStream driver
        port drvReceiveIn        = comStub.drvReceiveIn

        @ Output port returning ownership of the buffer that came in on drvReceiveIn back to the driver
        port drvReceiveReturnOut = comStub.drvReceiveReturnOut

        @ Output port sending framed data to the ByteStream driver for transmission
        port drvSendOut          = comStub.drvSendOut

        @ Input port receiving the ready signal when the ByteStream driver has connected
        port drvConnected        = comStub.drvConnected

        # Buffer management for ComDriver
        @ Input port for requesting (allocating) a new Fw::Buffer from the comms buffer pool
        port commsBufferGetCallee = commsBufferManager.bufferGetCallee

        @ Input port for deallocating Fw::Buffers back into the comms buffer pool
        port commsBufferSendIn    = commsBufferManager.bufferSendIn

        # Scheduling
        @ Input port for scheduling ComQueue telemetry output
        port comQueueRun          = comQueue.run

        @ Rate-group driven timeout to flush the ComAggregator buffer
        port aggregatorTimeout    = aggregator.timeout

        @ Input port triggering commsBufferManager telemetry output
        port bufferManagerSchedIn = commsBufferManager.schedIn

    } # end SpacePacket

    # This subtopology boxes the CCSDS TM/TC transfer frame layer: the TM framer (downlink),
    # and the frame accumulator + TC deframer (uplink).
    topology TmTcFraming {
        # Usage Note:
        #
        # When importing this subtopology, users shall establish the following external connections:
        #
        # 1) Upstream (packet layer, e.g. SpacePacketFraming):
        #     - [upstream].dataOut                          -> ComCcsds.TmTcFraming.dataIn
        #     - ComCcsds.TmTcFraming.dataReturnOut          -> [upstream].dataReturnIn
        #     - ComCcsds.TmTcFraming.comStatusOut           -> [upstream].comStatusIn
        #     - ComCcsds.TmTcFraming.dataOut                -> [upstream].dataIn (deframed data)
        #     - [upstream].dataReturnOut                    -> ComCcsds.TmTcFraming.dataReturnIn
        # 2) Downstream (a component implementing the Svc.Com interface):
        #     - ComCcsds.TmTcFraming.framedDataOut          -> [Svc.Com].dataIn
        #     - ComCcsds.TmTcFraming.framedDataReturnOut    -> [Svc.Com].dataReturnIn
        #     - [Svc.Com].dataReturnOut -> ComCcsds.TmTcFraming.framedDataReturnIn
        #     - [Svc.Com].comStatusOut  -> ComCcsds.TmTcFraming.framedComStatusIn
        #     - [Svc.Com].dataOut       -> ComCcsds.TmTcFraming.framedDataIn
        # 3) Buffer management (e.g. a Svc.BufferManager):
        #     - ComCcsds.TmTcFraming.bufferAllocate   -> [BufferManager].bufferGetCallee
        #     - ComCcsds.TmTcFraming.bufferDeallocate -> [BufferManager].bufferSendIn

        instance framer
        instance tcDeframer
        instance frameAccumulator

        connections Uplink {
            # FrameAccumulator <-> TcDeframer
            frameAccumulator.dataOut -> tcDeframer.dataIn
            tcDeframer.dataReturnOut -> frameAccumulator.dataReturnIn
        }

        # ----------------------------------------------------------------------
        # Topology ports
        # ----------------------------------------------------------------------

        # Upstream boundary (packet layer)
        @ Input port receiving space packets from the packet layer for TM framing
        port dataIn        = framer.dataIn

        @ Output port returning ownership of downlinked buffers to the packet layer
        port dataReturnOut = framer.dataReturnOut

        @ Output port forwarding com status to the packet layer
        port comStatusOut  = framer.comStatusOut

        @ Output port sending TC-deframed data to the packet layer
        port dataOut       = tcDeframer.dataOut

        @ Input port receiving back ownership of uplinked buffers from the packet layer
        port dataReturnIn  = tcDeframer.dataReturnIn

        # Downstream boundary (Svc.Com interface)
        @ Output port sending TM transfer frames to the com interface
        port framedDataOut       = framer.dataOut

        @ Input port receiving back ownership of transmitted frame buffers from the com interface
        port framedDataReturnIn  = framer.dataReturnIn

        @ Input port receiving com status from the com interface
        port framedComStatusIn   = framer.comStatusIn

        @ Input port receiving raw uplink data from the com interface
        port framedDataIn        = frameAccumulator.dataIn

        @ Output port returning ownership of received uplink buffers to the com interface
        port framedDataReturnOut = frameAccumulator.dataReturnOut

        # Buffer management boundary
        @ Output port for allocating accumulation buffers
        port bufferAllocate   = frameAccumulator.bufferAllocate

        @ Output port for deallocating accumulation buffers
        port bufferDeallocate = frameAccumulator.bufferDeallocate
    } # end TmTcFraming

    # This subtopology composes the SpacePacketFraming packet layer with the TmTcFraming
    # TM/TC transfer frame layer to form the full CCSDS communications stack.
    topology FramingSubtopology {
        # Usage Note:
        #
        # When importing this subtopology, users shall establish 5 port connections with a component implementing
        # the Svc.Com (Svc/Interfaces/Com.fpp) interface. They are as follows:
        #
        # 1) Outputs:
        #     - ComCcsds.FramingSubtopology.dataOut       -> [Svc.Com].dataIn
        #     - ComCcsds.FramingSubtopology.dataReturnOut -> [Svc.Com].dataReturnIn
        # 2) Inputs:
        #     - [Svc.Com].dataReturnOut -> ComCcsds.FramingSubtopology.dataReturnIn
        #     - [Svc.Com].comStatusOut  -> ComCcsds.FramingSubtopology.comStatusIn
        #     - [Svc.Com].dataOut       -> ComCcsds.FramingSubtopology.dataIn

        # Packet layer (router, ComQueue, space packet framer/deframer, buffer manager)
        import SpacePacketFraming

        # TM/TC transfer frame layer (TM framer, frame accumulator, TC deframer)
        import TmTcFraming

        connections Downlink {
            # SpacePacketFraming <-> TmTcFraming
            SpacePacketFraming.dataOut -> TmTcFraming.dataIn
            TmTcFraming.dataReturnOut  -> SpacePacketFraming.dataReturnIn

            # ComStatus
            TmTcFraming.comStatusOut -> SpacePacketFraming.comStatusIn
            # (Outgoing) TmTcFraming <-> ComInterface connections shall be established by the user
        }

        connections Uplink {
            # (Incoming) ComInterface <-> TmTcFraming connections shall be established by the user
            # TmTcFraming buffer allocations
            TmTcFraming.bufferDeallocate -> SpacePacketFraming.bufferSendIn
            TmTcFraming.bufferAllocate   -> SpacePacketFraming.bufferGetCallee
            # TmTcFraming <-> SpacePacketFraming
            TmTcFraming.dataOut               -> SpacePacketFraming.dataIn
            SpacePacketFraming.dataReturnOut  -> TmTcFraming.dataReturnIn
        }

        # ----------------------------------------------------------------------
        # Topology ports (Svc.Com boundary)
        # ----------------------------------------------------------------------

        @ Output port sending TM transfer frames to the com interface
        port dataOut       = framer.dataOut

        @ Input port receiving back ownership of transmitted frame buffers from the com interface
        port dataReturnIn  = framer.dataReturnIn

        @ Input port receiving com status from the com interface
        port comStatusIn   = framer.comStatusIn

        @ Input port receiving raw uplink data from the com interface
        port dataIn        = frameAccumulator.dataIn

        @ Output port returning ownership of received uplink buffers to the com interface
        port dataReturnOut = frameAccumulator.dataReturnOut
    } # end FramingSubtopology

    # This subtopology uses FramingSubtopology with a ComStub component for Com Interface
    topology Subtopology {
        import FramingSubtopology

        instance comStub

        connections ComStub {
            # FramingSubtopology <-> ComStub (Downlink)
            FramingSubtopology.dataOut -> comStub.dataIn
            comStub.dataReturnOut      -> FramingSubtopology.dataReturnIn
            comStub.comStatusOut       -> FramingSubtopology.comStatusIn

            # ComStub <-> FramingSubtopology (Uplink)
            comStub.dataOut -> FramingSubtopology.dataIn
            FramingSubtopology.dataReturnOut -> comStub.dataReturnIn
        }

        # ----------------------------------------------------------------------
        # Topology ports
        # ----------------------------------------------------------------------

        # Command routing
        @ Output port sending routed command packets to the command dispatcher
        port commandOut         = fprimeRouter.commandOut

        @ Input port receiving command response messages back into the router
        port cmdResponseIn      = fprimeRouter.cmdResponseIn

        @ Output port sending uplinked file packets to the file handling stack
        port fileUplinkOut          = fprimeRouter.fileOut

        @ Input port receiving back buffer ownership from the file handling stack
        port fileUplinkReturnIn = fprimeRouter.fileBufferReturnIn

        # Telemetry/events/file queuing (array ports - index at connection site)
        @ Input port array for queueing Fw::ComBuffers
        port comPacketQueueIn = comQueue.comPacketQueueIn

        @ Input port array for queueing Fw::Buffers
        port bufferQueueIn    = comQueue.bufferQueueIn

        @ Output port array returning ownership of Fw::Buffers to their original sender after dequeuing
        port bufferReturnOut  = comQueue.bufferReturnOut

        # ComDriver interface (via ComStub)
        @ Input port receiving data read from the ByteStream driver 
        port drvReceiveIn        = comStub.drvReceiveIn

        @ Output port returning ownership of the buffer that came in on drvReceiveIn back to the driver
        port drvReceiveReturnOut = comStub.drvReceiveReturnOut

        @ Output port sending framed data to the ByteStream driver for transmission
        port drvSendOut          = comStub.drvSendOut

        @ Input port receiving the ready signal when the ByteStream driver has connected
        port drvConnected        = comStub.drvConnected

        # Buffer management for ComDriver
        @ Input port for requesting (allocating) a new Fw::Buffer from the comms buffer pool
        port commsBufferGetCallee = commsBufferManager.bufferGetCallee

        @ Input port for deallocating Fw::Buffers back into the comms buffer pool
        port commsBufferSendIn    = commsBufferManager.bufferSendIn

        # Scheduling
        @ Input port for scheduling ComQueue telemetry output
        port comQueueRun          = comQueue.run

        @ Rate-group driven timeout to flush the ComAggregator buffer
        port aggregatorTimeout    = aggregator.timeout

        @ Input port triggering commsBufferManager telemetry output
        port bufferManagerSchedIn = commsBufferManager.schedIn

    } # end Subtopology

    # ----------------------------------------------------------------------
    # Segmented TC uplink variants (CCSDS 232.0-B-4 Segment Header / MAP packet extraction)
    # ----------------------------------------------------------------------

    # TC/TM transfer frame layer with the TC deframer in Segment Header mode.
    # Same topology ports as TmTcFraming so the *Segmented topologies wire identically.
    topology TmTcFramingSegmented {
        instance framer
        instance tcDeframerSeg
        instance frameAccumulator

        connections Uplink {
            # FrameAccumulator <-> TcDeframer (Segment Header mode)
            frameAccumulator.dataOut    -> tcDeframerSeg.dataIn
            tcDeframerSeg.dataReturnOut -> frameAccumulator.dataReturnIn
        }

        # ----------------------------------------------------------------------
        # Topology ports
        # ----------------------------------------------------------------------

        # Upstream boundary (packet layer) - downlink identical to TmTcFraming
        @ Input port receiving space packets from the packet layer for TM framing
        port dataIn        = framer.dataIn

        @ Output port returning ownership of downlinked buffers to the packet layer
        port dataReturnOut = framer.dataReturnOut

        @ Output port forwarding com status to the packet layer
        port comStatusOut  = framer.comStatusOut

        @ Output port sending TC-deframed frame data (Segment Header stripped into FrameContext)
        port dataOut       = tcDeframerSeg.dataOut

        @ Input port receiving back ownership of uplinked buffers
        port dataReturnIn  = tcDeframerSeg.dataReturnIn

        # Downstream boundary (Svc.Com interface) - identical to TmTcFraming
        @ Output port sending TM transfer frames to the com interface
        port framedDataOut       = framer.dataOut

        @ Input port receiving back ownership of transmitted frame buffers from the com interface
        port framedDataReturnIn  = framer.dataReturnIn

        @ Input port receiving com status from the com interface
        port framedComStatusIn   = framer.comStatusIn

        @ Input port receiving raw uplink data from the com interface
        port framedDataIn        = frameAccumulator.dataIn

        @ Output port returning ownership of received uplink buffers to the com interface
        port framedDataReturnOut = frameAccumulator.dataReturnOut

        # Buffer management boundary
        @ Output port for allocating accumulation buffers
        port bufferAllocate   = frameAccumulator.bufferAllocate

        @ Output port for deallocating accumulation buffers
        port bufferDeallocate = frameAccumulator.bufferDeallocate
    } # end TmTcFramingSegmented

    # MAP packet extraction layer: reassembles Space Packets from authenticated TC segments.
    # Sits between the TC deframer (or the SDLS SUCCESS gate) and SpacePacketFraming.
    topology TcMapExtraction {
        instance tcMapReassembler
        instance tcPacketBufferManager

        connections Pool {
            # Reassembled-packet buffers come from the dedicated pool only
            tcMapReassembler.allocate   -> tcPacketBufferManager.bufferGetCallee
            tcMapReassembler.deallocate -> tcPacketBufferManager.bufferSendIn
        }

        # ----------------------------------------------------------------------
        # Topology ports
        # ----------------------------------------------------------------------

        @ Segments in (from the TC deframer in Segment Header mode or from the SDLS decryption layer)
        port dataIn        = tcMapReassembler.dataIn

        @ Segment frame buffers returned upstream
        port dataReturnOut = tcMapReassembler.dataReturnOut

        @ Complete Space Packets out
        port dataOut       = tcMapReassembler.dataOut

        @ Packet buffers returned by the packet layer
        port dataReturnIn  = tcMapReassembler.dataReturnIn

        @ Optional: schedule the dedicated pool's telemetry (Svc.BufferManager.schedIn)
        port poolSchedIn   = tcPacketBufferManager.schedIn
    } # end TcMapExtraction

    # FramingSubtopology with the segmented TC uplink (no SDLS):
    # frameAccumulator -> tcDeframerSeg -> tcMapReassembler -> spacePacketDeframer
    topology FramingSubtopologySegmented {
        # Usage Note: same external connections as FramingSubtopology (see above).

        # Packet layer (router, ComQueue, space packet framer/deframer, buffer manager)
        import SpacePacketFraming

        # TM/TC transfer frame layer with the TC deframer in Segment Header mode
        import TmTcFramingSegmented

        # MAP packet extraction layer (reassembler + dedicated pool)
        import TcMapExtraction

        connections Downlink {
            # SpacePacketFraming <-> TmTcFramingSegmented
            SpacePacketFraming.dataOut          -> TmTcFramingSegmented.dataIn
            TmTcFramingSegmented.dataReturnOut  -> SpacePacketFraming.dataReturnIn

            # ComStatus
            TmTcFramingSegmented.comStatusOut   -> SpacePacketFraming.comStatusIn
            # (Outgoing) TmTcFramingSegmented <-> ComInterface connections shall be established by the user
        }

        connections Uplink {
            # (Incoming) ComInterface <-> TmTcFramingSegmented connections shall be established by the user
            # TmTcFramingSegmented buffer allocations
            TmTcFramingSegmented.bufferDeallocate -> SpacePacketFraming.bufferSendIn
            TmTcFramingSegmented.bufferAllocate   -> SpacePacketFraming.bufferGetCallee

            # TC deframer (Segment Header mode) -> MAP reassembler
            TmTcFramingSegmented.dataOut  -> TcMapExtraction.dataIn
            TcMapExtraction.dataReturnOut -> TmTcFramingSegmented.dataReturnIn

            # MAP reassembler -> Space Packet deframer
            TcMapExtraction.dataOut             -> SpacePacketFraming.dataIn
            SpacePacketFraming.dataReturnOut    -> TcMapExtraction.dataReturnIn
        }

        # ----------------------------------------------------------------------
        # Topology ports (Svc.Com boundary)
        # ----------------------------------------------------------------------

        @ Output port sending TM transfer frames to the com interface
        port dataOut       = framer.dataOut

        @ Input port receiving back ownership of transmitted frame buffers from the com interface
        port dataReturnIn  = framer.dataReturnIn

        @ Input port receiving com status from the com interface
        port comStatusIn   = framer.comStatusIn

        @ Input port receiving raw uplink data from the com interface
        port dataIn        = frameAccumulator.dataIn

        @ Output port returning ownership of received uplink buffers to the com interface
        port dataReturnOut = frameAccumulator.dataReturnOut
    } # end FramingSubtopologySegmented

    # Subtopology (with ComStub) with the segmented TC uplink. Same topology ports as
    # Subtopology plus tcPacketBufferManagerSchedIn.
    topology SegmentedSubtopology {
        import FramingSubtopologySegmented

        instance comStub

        connections ComStub {
            # FramingSubtopologySegmented <-> ComStub (Downlink)
            FramingSubtopologySegmented.dataOut -> comStub.dataIn
            comStub.dataReturnOut  -> FramingSubtopologySegmented.dataReturnIn
            comStub.comStatusOut   -> FramingSubtopologySegmented.comStatusIn

            # ComStub <-> FramingSubtopologySegmented (Uplink)
            comStub.dataOut        -> FramingSubtopologySegmented.dataIn
            FramingSubtopologySegmented.dataReturnOut -> comStub.dataReturnIn
        }

        # ----------------------------------------------------------------------
        # Topology ports (identical to Subtopology, plus tcPacketBufferManagerSchedIn)
        # ----------------------------------------------------------------------

        # Command routing
        @ Output port sending routed command packets to the command dispatcher
        port commandOut         = fprimeRouter.commandOut

        @ Input port receiving command response messages back into the router
        port cmdResponseIn      = fprimeRouter.cmdResponseIn

        @ Output port sending uplinked file packets to the file handling stack
        port fileUplinkOut          = fprimeRouter.fileOut

        @ Input port receiving back buffer ownership from the file handling stack
        port fileUplinkReturnIn = fprimeRouter.fileBufferReturnIn

        # Telemetry/events/file queuing (array ports - index at connection site)
        @ Input port array for queueing Fw::ComBuffers
        port comPacketQueueIn = comQueue.comPacketQueueIn

        @ Input port array for queueing Fw::Buffers
        port bufferQueueIn    = comQueue.bufferQueueIn

        @ Output port array returning ownership of Fw::Buffers to their original sender after dequeuing
        port bufferReturnOut  = comQueue.bufferReturnOut

        # ComDriver interface (via ComStub)
        @ Input port receiving data read from the ByteStream driver
        port drvReceiveIn        = comStub.drvReceiveIn

        @ Output port returning ownership of the buffer that came in on drvReceiveIn back to the driver
        port drvReceiveReturnOut = comStub.drvReceiveReturnOut

        @ Output port sending framed data to the ByteStream driver for transmission
        port drvSendOut          = comStub.drvSendOut

        @ Input port receiving the ready signal when the ByteStream driver has connected
        port drvConnected        = comStub.drvConnected

        # Buffer management for ComDriver
        @ Input port for requesting (allocating) a new Fw::Buffer from the comms buffer pool
        port commsBufferGetCallee = commsBufferManager.bufferGetCallee

        @ Input port for deallocating Fw::Buffers back into the comms buffer pool
        port commsBufferSendIn    = commsBufferManager.bufferSendIn

        # Scheduling
        @ Input port for scheduling ComQueue telemetry output
        port comQueueRun          = comQueue.run

        @ Rate-group driven timeout to flush the ComAggregator buffer
        port aggregatorTimeout    = aggregator.timeout

        @ Input port triggering commsBufferManager telemetry output
        port bufferManagerSchedIn = commsBufferManager.schedIn

        @ Input port triggering the dedicated TC packet pool (tcPacketBufferManager) telemetry output
        port tcPacketBufferManagerSchedIn = tcPacketBufferManager.schedIn

    } # end SegmentedSubtopology

} # end ComCcsds
