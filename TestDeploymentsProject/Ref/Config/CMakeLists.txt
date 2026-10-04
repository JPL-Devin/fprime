####
# Ref configuration overrides:
#
# Files listed here replace the F Prime configuration files of the same name. The Ref deployment overrides the fault
# protection configuration to add the COUNTER_HIGH fault, its multi-step sequence response, to replace CdhCore's
# FatalHandler with FatalToFault so that FATALs are handled as faults, and to deepen CdhCore's event queue so that the
# burst of events a multi-step response emits within one tick is not dropped.
####
register_fprime_config(
    CONFIGURATION_OVERRIDES
        "${CMAKE_CURRENT_LIST_DIR}/FaultConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreFatalHandlerConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreConfig.fpp"
    INTERFACE
)
