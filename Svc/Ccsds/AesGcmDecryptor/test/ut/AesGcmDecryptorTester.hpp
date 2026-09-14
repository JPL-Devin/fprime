// ======================================================================
// \title  AesGcmDecryptorTester.hpp
// \author vivi and claradavisb
// \brief  hpp file for AesGcmDecryptor component test harness implementation class
// ======================================================================

#ifndef Svc_Ccsds_AesGcmDecryptorTester_HPP
#define Svc_Ccsds_AesGcmDecryptorTester_HPP

#include "Svc/Ccsds/AesGcmDecryptor/AesGcmDecryptor.hpp"
#include "Svc/Ccsds/AesGcmDecryptor/AesGcmDecryptorGTestBase.hpp"

namespace Svc {

namespace Ccsds {

class AesGcmDecryptorTester final : public AesGcmDecryptorGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    //! Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 10;

    //! Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Length of an AES-256 key, in bytes
    static const FwSizeType AES_256_KEY_LEN = 32;

    //! Length of the AES-GCM initialization vector, in bytes
    static const FwSizeType GCM_IV_LEN = 12;

    //! Length of the AES-GCM authentication tag (the SDLS MAC), in bytes
    static const FwSizeType GCM_TAG_LEN = 16;

    //! Virtual channel the component is configured for in these tests
    static const U8 TEST_VC_ID = 5;

    //! Security association index passed on decryptIn in these tests
    static const U16 TEST_SPI = 0x1234;

    //! Backing storage for buffers handed to the component
    static const FwSizeType TEST_BUFFER_SIZE = 256;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object AesGcmDecryptorTester
    AesGcmDecryptorTester();

    //! Destroy object AesGcmDecryptorTester
    ~AesGcmDecryptorTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! A frame built by an implementation outside this repository decrypts to the expected
    //! plaintext. Covers SVC-CCSDS-AES-DECRYPTOR-001 and SVC-CCSDS-AES-DECRYPTOR-002.
    void testKnownAnswer();

    //! Svc::Ccsds::Utils::SdlsTcAuthMask agrees, byte for byte, with the mask this harness
    //! builds from the ground segment's contract. Covers SVC-CCSDS-AES-DECRYPTOR-002.
    void testAuthMaskLayout();

    //! A well-formed frame decrypts in place, leaving the plaintext at IV_LEN into the
    //! original allocation. Covers SVC-CCSDS-AES-DECRYPTOR-001.
    void testDecryptNominal();

    //! A frame carrying no ciphertext at all is still authenticated and accepted.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-001.
    void testEmptyCiphertext();

    //! The emitted buffer keeps the allocation context and origin of the buffer that arrived,
    //! so it remains deallocatable. Covers SVC-CCSDS-AES-DECRYPTOR-006.
    void testAllocationContextPreserved();

    //! A single flipped ciphertext bit fails the MAC check. Covers SVC-CCSDS-AES-DECRYPTOR-003.
    void testTamperedCiphertext();

    //! A single flipped MAC bit fails the MAC check. Covers SVC-CCSDS-AES-DECRYPTOR-003.
    void testTamperedMac();

    //! A single flipped IV bit fails the MAC check. Covers SVC-CCSDS-AES-DECRYPTOR-003.
    void testTamperedIv();

    //! A frame authenticated for another virtual channel is rejected, which is the whole
    //! point of putting the VC in the AAD. Covers SVC-CCSDS-AES-DECRYPTOR-002.
    void testWrongVcId();

    //! A frame authenticated under another security association is rejected.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-002.
    void testWrongSecurityAssociation();

    //! The virtual channel authenticated in the AAD comes from the frame context, so frames
    //! for different VCs authenticate independently. Covers SVC-CCSDS-AES-DECRYPTOR-002.
    void testVcFromContext();

    //! A good frame still decrypts after a rejected one. The cipher context is built once and
    //! reused, so a failed MAC check must not leave it unusable. Covers SVC-CCSDS-AES-DECRYPTOR-003.
    void testRecoversAfterMacFailure();

    //! A buffer too short to hold an IV and a MAC is rejected without touching the key.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-004.
    void testShortBuffer();

    //! A key the key manager could not supply yields KEY_ERROR.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-005.
    void testKeyUnavailable();

    //! A key of the wrong length yields KEY_ERROR rather than decrypting under it.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-005.
    void testWrongKeySize();

    //! A buffer returned on decryptReturnIn goes back to its sender.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-007.
    void testBufferReturn();

    //! Without a Segment Header both constructors build the same 19-byte AAD and the Segment
    //! Header argument is ignored. Covers SVC-CCSDS-AES-DECRYPTOR-002.
    void testAuthMaskNoSh();

    //! With a Segment Header the AAD is 20 bytes with the received octet between the primary
    //! header and the SPI. Covers SVC-CCSDS-AES-DECRYPTOR-008.
    void testAuthMaskSh();

    //! The FIRST Segment Header known-answer frame decrypts to the expected plaintext.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-008.
    void testDecryptVectorFirst();

    //! The UNSEGMENTED Segment Header known-answer frame decrypts.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-008.
    void testDecryptVectorUnsegmented();

    //! The CONTINUING and LAST Segment Header known-answer frames decrypt.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-008.
    void testDecryptVectorContinuingLast();

    //! A frame authenticated with a Segment Header fails under a context saying there was
    //! none, and vice versa: the Segment Header octet is part of the AAD.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-008.
    void testShNotInAadFails();

    //! A frame whose context carries a Segment Header other than the one authenticated (MAP
    //! or sequence flags) fails the MAC check. Covers SVC-CCSDS-AES-DECRYPTOR-008.
    void testShTamperFails();

    //! One instance decrypts a 19-byte-AAD frame and then a 20-byte-AAD frame, which is only
    //! possible when the runtime AAD length reaches the cipher.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-008.
    void testAadLengthPassed();

    //! An end-to-end SDLS frame on VC 0 with a Segment Header decrypts.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-002 and SVC-CCSDS-AES-DECRYPTOR-008.
    void testDecryptFrameVc0();

    //! Both end-to-end SDLS frames on VC 1 decrypt, and the first fails under a VC 0 context.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-002 and SVC-CCSDS-AES-DECRYPTOR-008.
    void testDecryptFrameVc1();

    //! An end-to-end frame whose MAC was flipped (and whose FECF was recomputed so it passed
    //! the CRC check upstream) fails the MAC check. Covers SVC-CCSDS-AES-DECRYPTOR-003.
    void testTamperedMacVector();

    //! Frames with and without a Segment Header, and with differing Segment Headers, alternate
    //! through one instance and each authenticates: the AAD is built per frame.
    //! Covers SVC-CCSDS-AES-DECRYPTOR-008.
    void testPerFrameAad();

  private:
    // ----------------------------------------------------------------------
    // Handler overrides
    // ----------------------------------------------------------------------

    //! Stand in for the key manager, supplying whatever setKey() last configured
    Svc::Ccsds::SdlsStatus from_keyGet_handler(FwIndexType portNum,
                                               U16 securityAssociationIndex,
                                               Svc::Ccsds::SdlsKeyBuffer& key) override;

    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Set the key, and the status, this harness returns on keyGet
    void setKey(const U8* key, FwSizeType keyLen, Svc::Ccsds::SdlsStatus status);

    //! Build IV | ciphertext | MAC into the harness storage for the given plaintext, and
    //! return a buffer wrapping it; the AAD carries the Segment Header when present
    Fw::Buffer buildFrame(const U8* plaintext,
                          FwSizeType plainLen,
                          U8 vcId,
                          U16 spi,
                          bool segmentHeaderPresent = false,
                          U8 segmentHeader = 0);

    //! Copy a fixed IV | ciphertext | MAC vector into the harness storage and return a
    //! buffer wrapping it
    Fw::Buffer loadFrame(const U8* iv, const U8* ciphertext, FwSizeType cipherLen, const U8* mac);

    //! Hand a frame to the component, naming the virtual channel and the stripped Segment
    //! Header on the context
    void sendDecrypt(Fw::Buffer& data,
                     U16 spi,
                     U8 vcId = TEST_VC_ID,
                     bool segmentHeaderPresent = false,
                     U8 segmentHeader = 0);

    //! Assert that exactly one buffer came out on decryptOut carrying the given status
    void assertStatus(Svc::Ccsds::SdlsStatus status);

    //! Assert that exactly one buffer came out on decryptOut carrying the given plaintext
    void assertPlaintext(const U8* expected, FwSizeType expectedLen);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    AesGcmDecryptor component;

    //! Key handed out on keyGet
    U8 m_key[AES_256_KEY_LEN];

    //! Length of the key handed out on keyGet
    FwSizeType m_keyLen;

    //! Status returned on keyGet
    Svc::Ccsds::SdlsStatus m_keyStatus;

    //! Backing storage for the frames handed to the component
    U8 m_storage[TEST_BUFFER_SIZE];
};

}  // namespace Ccsds

}  // namespace Svc

#endif
