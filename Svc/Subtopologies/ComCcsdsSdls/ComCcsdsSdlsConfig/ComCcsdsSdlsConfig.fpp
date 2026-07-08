module ComCcsdsSdlsConfig {
    #Base ID for the ComCcsdsSdls Subtopology, all components are offsets from this base ID
    constant BASE_ID = 0x06000000

    module QueueSizes {
        constant comQueue    = 50
        constant aggregator  = 10
    }

    module StackSizes {
        constant comQueue   = 64 * 1024
        constant aggregator = 64 * 1024
    }

    module Priorities {
        constant aggregator = 30
        constant comQueue   = 29
    }

    # Queue configuration constants
    module QueueDepths {
        constant events      = 200
        constant tlm         = 500
        constant file        = 100
    }

    module QueuePriorities {
        constant events      = 0
        constant tlm         = 2
        constant file        = 1
    }

    # Buffer management constants
    module BuffMgr {
        constant frameAccumulatorSize  = 2048
        constant commsBuffSize         = 2048
        constant commsFileBuffSize     = 3000
        constant commsBuffCount        = 20
        constant commsFileBuffCount    = 30
        constant commsBuffMgrId        = 200
    }
}

module ComCcsdsSdls {
    @ Default decryptor handling the base security association (SdlsSaRouter port 0).
    @ Defined in the configuration module so projects may override the configuration
    @ to select a different decryptor implementation.
    @
    @ WARNING: the default Svc.Ccsds.ClearTextDecryptor provides NO security: no
    @ confidentiality, no integrity, and no authentication.
    instance decryptor: Svc.Ccsds.ClearTextDecryptor base id ComCcsdsSdlsConfig.BASE_ID + 0x0D000
}
