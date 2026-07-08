module ComCcsdsSdls {

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
    instance comQueue: Svc.ComQueue base id ComCcsdsSdlsConfig.BASE_ID + 0x00000 \
        queue size ComCcsdsSdlsConfig.QueueSizes.comQueue \
        stack size ComCcsdsSdlsConfig.StackSizes.comQueue \
        priority ComCcsdsSdlsConfig.Priorities.comQueue \
    {
        phase Fpp.ToCpp.Phases.configComponents """
        using namespace ComCcsdsSdls;
        Svc::ComQueue::QueueConfigurationTable configurationTable;

        // Events (highest-priority)
        configurationTable.entries[Ports_ComPacketQueue::EVENTS].depth = ComCcsdsSdlsConfig::QueueDepths::events;
        configurationTable.entries[Ports_ComPacketQueue::EVENTS].priority = ComCcsdsSdlsConfig::QueuePriorities::events;

        // Telemetry
        configurationTable.entries[Ports_ComPacketQueue::TELEMETRY].depth = ComCcsdsSdlsConfig::QueueDepths::tlm;
        configurationTable.entries[Ports_ComPacketQueue::TELEMETRY].priority = ComCcsdsSdlsConfig::QueuePriorities::tlm;

        // File Downlink Queue (buffer queue using NUM_CONSTANTS offset)
        configurationTable.entries[Ports_ComPacketQueue::NUM_CONSTANTS + Ports_ComBufferQueue::FILE].depth = ComCcsdsSdlsConfig::QueueDepths::file;
        configurationTable.entries[Ports_ComPacketQueue::NUM_CONSTANTS + Ports_ComBufferQueue::FILE].priority = ComCcsdsSdlsConfig::QueuePriorities::file;

        // Allocation identifier is 0 as the MallocAllocator discards it
        ComCcsdsSdls::comQueue.configure(configurationTable, 0, ComCcsdsSdls::Allocation::memAllocator);
        """
        phase Fpp.ToCpp.Phases.tearDownComponents """
        ComCcsdsSdls::comQueue.cleanup();
        """
    }

    # ----------------------------------------------------------------------
    # Passive Components
    # ----------------------------------------------------------------------
    instance frameAccumulator: Svc.FrameAccumulator base id ComCcsdsSdlsConfig.BASE_ID + 0x01000 \
    {

        phase Fpp.ToCpp.Phases.configObjects """
        Svc::FrameDetectors::CcsdsTcFrameDetector frameDetector;
        """
        phase Fpp.ToCpp.Phases.configComponents """
        ComCcsdsSdls::frameAccumulator.configure(
            ConfigObjects::ComCcsdsSdls_frameAccumulator::frameDetector,
            1,
            ComCcsdsSdls::Allocation::memAllocator,
            ComCcsdsSdlsConfig::BuffMgr::frameAccumulatorSize
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        ComCcsdsSdls::frameAccumulator.cleanup();
        """
    }

    instance commsBufferManager: Svc.BufferManager base id ComCcsdsSdlsConfig.BASE_ID + 0x02000 \
    {
        phase Fpp.ToCpp.Phases.configObjects """
        Svc::BufferManager::BufferBins bins;
        """

        phase Fpp.ToCpp.Phases.configComponents """
        memset(&ConfigObjects::ComCcsdsSdls_commsBufferManager::bins, 0, sizeof(ConfigObjects::ComCcsdsSdls_commsBufferManager::bins));
        ConfigObjects::ComCcsdsSdls_commsBufferManager::bins.bins[0].bufferSize = ComCcsdsSdlsConfig::BuffMgr::commsBuffSize;
        ConfigObjects::ComCcsdsSdls_commsBufferManager::bins.bins[0].numBuffers = ComCcsdsSdlsConfig::BuffMgr::commsBuffCount;
        ConfigObjects::ComCcsdsSdls_commsBufferManager::bins.bins[1].bufferSize = ComCcsdsSdlsConfig::BuffMgr::commsFileBuffSize;
        ConfigObjects::ComCcsdsSdls_commsBufferManager::bins.bins[1].numBuffers = ComCcsdsSdlsConfig::BuffMgr::commsFileBuffCount;
        ComCcsdsSdls::commsBufferManager.setup(
            ComCcsdsSdlsConfig::BuffMgr::commsBuffMgrId,
            0,
            ComCcsdsSdls::Allocation::memAllocator,
            ConfigObjects::ComCcsdsSdls_commsBufferManager::bins
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        ComCcsdsSdls::commsBufferManager.cleanup();
        """
    }

    instance fprimeRouter: Svc.FprimeRouter base id ComCcsdsSdlsConfig.BASE_ID + 0x03000

    instance tcDeframer: Svc.Ccsds.TcDeframer base id ComCcsdsSdlsConfig.BASE_ID + 0x04000

    instance spacePacketDeframer: Svc.Ccsds.SpacePacketDeframer base id ComCcsdsSdlsConfig.BASE_ID + 0x05000

    instance aggregator: Svc.ComAggregator base id ComCcsdsSdlsConfig.BASE_ID + 0x06000 \
        queue size ComCcsdsSdlsConfig.QueueSizes.aggregator \
        stack size ComCcsdsSdlsConfig.StackSizes.aggregator \
        priority ComCcsdsSdlsConfig.Priorities.aggregator

    # NOTE: name 'framer' is used for the framer that connects to the Com Adapter Interface for better subtopology interoperability
    instance framer: Svc.Ccsds.TmFramer base id ComCcsdsSdlsConfig.BASE_ID + 0x07000

    instance spacePacketFramer: Svc.Ccsds.SpacePacketFramer base id ComCcsdsSdlsConfig.BASE_ID + 0x08000

    instance apidManager: Svc.Ccsds.ApidManager base id ComCcsdsSdlsConfig.BASE_ID + 0x09000

    instance comStub: Svc.ComStub base id ComCcsdsSdlsConfig.BASE_ID + 0x0A000

    instance sdlsDeframer: Svc.Ccsds.CcsdsSdlsDeframer base id ComCcsdsSdlsConfig.BASE_ID + 0x0B000

    instance saRouter: Svc.Ccsds.SdlsSaRouter base id ComCcsdsSdlsConfig.BASE_ID + 0x0C000

    # NOTE: the 'decryptor' instance is defined in the ComCcsdsSdlsConfig configuration
    # module, allowing projects to override the configuration and select a different
    # decryptor implementation. The default is Svc.Ccsds.ClearTextDecryptor (NO security).

    topology FramingSubtopology {
        # Usage Note:
        #
        # When importing this subtopology, users shall establish 5 port connections with a component implementing
        # the Svc.Com (Svc/Interfaces/Com.fpp) interface. They are as follows:
        #
        # 1) Outputs:
        #     - ComCcsdsSdls.framer.dataOut                 -> [Svc.Com].dataIn
        #     - ComCcsdsSdls.frameAccumulator.dataReturnOut -> [Svc.Com].dataReturnIn
        # 2) Inputs:
        #     - [Svc.Com].dataReturnOut -> ComCcsdsSdls.framer.dataReturnIn
        #     - [Svc.Com].comStatusOut  -> ComCcsdsSdls.framer.comStatusIn
        #     - [Svc.Com].dataOut       -> ComCcsdsSdls.frameAccumulator.dataIn


        # Active Components
        instance comQueue

        # Passive Components
        instance commsBufferManager
        instance frameAccumulator
        instance fprimeRouter
        instance tcDeframer
        instance spacePacketDeframer
        instance framer
        instance spacePacketFramer
        instance apidManager
        instance aggregator
        instance sdlsDeframer
        instance saRouter
        instance decryptor

        connections Downlink {
            # ComQueue <-> SpacePacketFramer
            comQueue.dataOut                -> spacePacketFramer.dataIn
            spacePacketFramer.dataReturnOut -> comQueue.dataReturnIn
            # SpacePacketFramer buffer and APID management
            spacePacketFramer.bufferAllocate   -> commsBufferManager.bufferGetCallee
            spacePacketFramer.bufferDeallocate -> commsBufferManager.bufferSendIn
            spacePacketFramer.getApidSeqCount  -> apidManager.getApidSeqCountIn
            # SpacePacketFramer <-> TmFramer
            spacePacketFramer.dataOut -> aggregator.dataIn
            aggregator.dataOut        -> framer.dataIn

            framer.dataReturnOut      -> aggregator.dataReturnIn
            aggregator.dataReturnOut    -> spacePacketFramer.dataReturnIn

            # ComStatus
            framer.comStatusOut            -> aggregator.comStatusIn
            aggregator.comStatusOut        -> spacePacketFramer.comStatusIn
            spacePacketFramer.comStatusOut -> comQueue.comStatusIn
            # (Outgoing) Framer <-> ComInterface connections shall be established by the user
        }

        connections Uplink {
            # (Incoming) ComInterface <-> FrameAccumulator connections shall be established by the user
            # FrameAccumulator buffer allocations
            frameAccumulator.bufferDeallocate -> commsBufferManager.bufferSendIn
            frameAccumulator.bufferAllocate   -> commsBufferManager.bufferGetCallee
            # FrameAccumulator <-> TcDeframer
            frameAccumulator.dataOut -> tcDeframer.dataIn
            tcDeframer.dataReturnOut -> frameAccumulator.dataReturnIn
            # TcDeframer <-> CcsdsSdlsDeframer (SDLS decryption step)
            tcDeframer.dataOut         -> sdlsDeframer.dataIn
            sdlsDeframer.dataReturnOut -> tcDeframer.dataReturnIn
            # CcsdsSdlsDeframer <-> SpacePacketDeframer
            sdlsDeframer.dataOut              -> spacePacketDeframer.dataIn
            spacePacketDeframer.dataReturnOut -> sdlsDeframer.dataReturnIn
            # SpacePacketDeframer APID validation
            spacePacketDeframer.validateApidSeqCount -> apidManager.validateApidSeqCountIn
            # SpacePacketDeframer <-> Router
            spacePacketDeframer.dataOut -> fprimeRouter.dataIn
            fprimeRouter.dataReturnOut  -> spacePacketDeframer.dataReturnIn
        }

        connections Decryption {
            # CcsdsSdlsDeframer <-> SdlsSaRouter (decryption requests and returns)
            sdlsDeframer.decryptOut       -> saRouter.decryptIn
            saRouter.decryptOut           -> sdlsDeframer.decryptIn
            sdlsDeframer.decryptReturnOut -> saRouter.decryptReturnIn
            saRouter.bufferReturnOut      -> sdlsDeframer.bufferReturnIn

            # SdlsSaRouter <-> default decryptor (port 0, base SA)
            saRouter.saDecryptOut[0]       -> decryptor.decryptIn
            decryptor.decryptOut           -> saRouter.saDecryptIn[0]
            saRouter.saDecryptReturnOut[0] -> decryptor.decryptReturnIn
            decryptor.bufferReturnOut      -> saRouter.saBufferReturnIn[0]
        }
    } # end FramingSubtopology

    # This subtopology uses FramingSubtopology with a ComStub component for Com Interface
    topology Subtopology {
        import FramingSubtopology

        instance comStub

        connections ComStub {
            # Framer <-> ComStub (Downlink)
            ComCcsdsSdls.framer.dataOut -> comStub.dataIn
            comStub.dataReturnOut       -> ComCcsdsSdls.framer.dataReturnIn
            comStub.comStatusOut        -> ComCcsdsSdls.framer.comStatusIn

            # ComStub <-> FrameAccumulator (Uplink)
            comStub.dataOut -> ComCcsdsSdls.frameAccumulator.dataIn
            ComCcsdsSdls.frameAccumulator.dataReturnOut -> comStub.dataReturnIn
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

} # end ComCcsdsSdls
