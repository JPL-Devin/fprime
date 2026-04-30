module Ref {

    @ Reference component implemented in Rust via fprime-rust.
    @ Demonstrates one of each: command, event, telemetry channel, parameter.
    @ fprime-rust
    queued component RustExample {

        # ----------------------------------------------------------------------
        # General Ports
        # ----------------------------------------------------------------------

        @ Rate-group driven step.  Used to bump the counter and re-emit telemetry.
        sync input port run: Svc.Sched

        # ----------------------------------------------------------------------
        # Special ports required for commands / events / telemetry / parameters
        # ----------------------------------------------------------------------

        @ Time get port -- enables event time stamping.
        time get port timeCaller

        @ Standard event log port.
        import Fw.Event

        @ Standard command interfaces.
        import Fw.Command

        @ Standard channel telemetry interface.
        import Fw.Channel

        @ Standard parameter interfaces.
        import Fw.Prm

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------

        @ Reset the running counter back to zero.
        async command Reset \
            opcode 0

        @ Increment the running counter by an explicit amount.
        async command Bump(
                              amount: U32
                            ) \
            opcode 1

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ Emitted on every Reset.  Helps verify command round-trip in tests.
        event ResetIssued \
            severity activity low \
            id 0 \
            format "RustExample reset issued"

        @ Emitted on every Bump with the new counter value.
        event Bumped(
                       value: U32
                     ) \
            severity activity low \
            id 1 \
            format "RustExample counter bumped to {}"

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Current value of the running counter.
        telemetry Counter: U32 id 0

        # ----------------------------------------------------------------------
        # Parameters
        # ----------------------------------------------------------------------

        @ Maximum value the counter is allowed to reach before saturating.
        param Threshold: U32 default 100 id 0

    }

}
