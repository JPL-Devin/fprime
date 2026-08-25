module Svc {
module Ccsds {
    @ Framer for the Unified Space Data Link Protocol (CCSDS Standard)
    passive component UslpFramer {

        import Framer

        @ Port for requesting the current time
        time get port timeCaller

    }
}
}
