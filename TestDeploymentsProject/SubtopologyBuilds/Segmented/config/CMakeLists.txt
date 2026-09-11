####
# Configuration override for the SDLS variant of the Segmented deployment: replaces
# Svc/Subtopologies/ComCcsdsSdls/ComCcsdsSdlsConfig/ComCcsdsSdlsConfig.fpp in its entirety so
# that ComCcsdsSdls.decryptor is Svc.Ccsds.AesGcmDecryptor (MAC verified) instead of the
# default clear-text decryptor. Only added to the build when SEGMENTED_SDLS is ON.
#
# Svc_Ccsds_AesGcmDecryptor is registered only when OpenSSL >= 3.5 is found
# (Svc/Ccsds/AesGcmDecryptor/CMakeLists.txt); pass -DOPENSSL_ROOT_DIR=<prefix> otherwise.
####
register_fprime_config(
    CONFIGURATION_OVERRIDES
        "${CMAKE_CURRENT_LIST_DIR}/ComCcsdsSdlsConfig.fpp"
    INTERFACE
    DEPENDS
        Svc_Ccsds_AesGcmDecryptor
        Svc_Ccsds_ClearTextEncryptor
        Svc_Ccsds_SdlsFileKeyManager
)
