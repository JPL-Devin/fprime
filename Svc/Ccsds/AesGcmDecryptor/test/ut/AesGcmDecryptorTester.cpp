// ======================================================================
// \title  AesGcmDecryptorTester.cpp
// \author vivi and claradavisb
// \brief  cpp file for AesGcmDecryptor component test harness implementation class
// ======================================================================

#include "AesGcmDecryptorTester.hpp"
#include "STest/Pick/Pick.hpp"
#include "Svc/Ccsds/Utils/SdlsAuthMask.hpp"

#include <openssl/evp.h>
#include <cstring>

namespace Svc {

namespace Ccsds {

const FwSizeType AesGcmDecryptorTester::MAX_HISTORY_SIZE;
const FwEnumStoreType AesGcmDecryptorTester::TEST_INSTANCE_ID;
const FwSizeType AesGcmDecryptorTester::AES_256_KEY_LEN;
const FwSizeType AesGcmDecryptorTester::GCM_IV_LEN;
const FwSizeType AesGcmDecryptorTester::GCM_TAG_LEN;
const U8 AesGcmDecryptorTester::TEST_VC_ID;
const U16 AesGcmDecryptorTester::TEST_SPI;
const FwSizeType AesGcmDecryptorTester::TEST_BUFFER_SIZE;

namespace {

// ----------------------------------------------------------------------
// Known-answer vector
// ----------------------------------------------------------------------

const U8 KAT_KEY[AesGcmDecryptorTester::AES_256_KEY_LEN] = {
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F};

const U8 KAT_IV[AesGcmDecryptorTester::GCM_IV_LEN] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                                      0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};

const U8 KAT_PLAINTEXT[28] = {0x53, 0x44, 0x4C, 0x53, 0x20, 0x41, 0x45, 0x53, 0x2D, 0x32, 0x35, 0x36, 0x2D, 0x47,
                              0x43, 0x4D, 0x20, 0x4B, 0x41, 0x54, 0x20, 0x70, 0x61, 0x79, 0x6C, 0x6F, 0x61, 0x64};

//! Ciphertext for KAT_PLAINTEXT under KAT_KEY and KAT_IV
const U8 KAT_CIPHERTEXT[28] = {0x69, 0x41, 0x20, 0x2B, 0x57, 0xCD, 0x01, 0xF3, 0xEF, 0x20, 0xB5, 0xF4, 0x95, 0xAA,
                               0x8F, 0xF6, 0xA3, 0x8F, 0x6C, 0x5D, 0x4C, 0x3C, 0xCC, 0x2A, 0x82, 0x4F, 0x89, 0xF2};

//! MAC over the above with the TC AAD for VC 5, SPI 0x1234
const U8 KAT_MAC[AesGcmDecryptorTester::GCM_TAG_LEN] = {0xC4, 0xB0, 0x91, 0x03, 0x7F, 0xA7, 0xA4, 0xAB,
                                                        0xD7, 0x25, 0xCB, 0xA2, 0xE8, 0x24, 0x08, 0xA6};

// ----------------------------------------------------------------------
// Segment Header known-answer vectors: same key, IV, plaintext and ciphertext as above,
// VC 5, SPI 0x1234, MAP 1, with the received Segment Header octet in the 20-byte AAD
// (00 00 14 00 00 | SH | 12 34 | 00 x12). Generated with an independent implementation
// (python cryptography AES-256-GCM).
// ----------------------------------------------------------------------

//! Sequence flags FIRST (01), MAP 1
const U8 KAT_SH_FIRST = 0x41;
//! Sequence flags UNSEGMENTED (11), MAP 1
const U8 KAT_SH_UNSEGMENTED = 0xC1;
//! Sequence flags CONTINUING (00), MAP 1
const U8 KAT_SH_CONTINUING = 0x01;
//! Sequence flags LAST (10), MAP 1
const U8 KAT_SH_LAST = 0x81;

const U8 KAT_MAC_FIRST[AesGcmDecryptorTester::GCM_TAG_LEN] = {0x52, 0x37, 0x0D, 0xDF, 0x09, 0xC8, 0x59, 0xAA,
                                                              0x82, 0xCB, 0x2A, 0x99, 0xFE, 0x9B, 0xD6, 0x08};

const U8 KAT_MAC_UNSEGMENTED[AesGcmDecryptorTester::GCM_TAG_LEN] = {0x07, 0xBE, 0xF4, 0x29, 0x81, 0x64, 0xEA, 0x83,
                                                                    0xC0, 0x73, 0xBD, 0x87, 0x56, 0x76, 0x59, 0x66};

const U8 KAT_MAC_CONTINUING[AesGcmDecryptorTester::GCM_TAG_LEN] = {0x78, 0xF3, 0xF1, 0x24, 0x4D, 0x9E, 0x00, 0x3E,
                                                                   0x23, 0x97, 0x61, 0x16, 0xAA, 0xED, 0x11, 0xBF};

const U8 KAT_MAC_LAST[AesGcmDecryptorTester::GCM_TAG_LEN] = {0x2D, 0x7A, 0x08, 0xD2, 0xC5, 0x32, 0xB3, 0x17,
                                                             0x61, 0x2F, 0xF6, 0x08, 0x02, 0x00, 0x9E, 0xD1};

// ----------------------------------------------------------------------
// End-to-end SDLS frames, SCID 0x044, SPI 0x0001, MAP 0, IV = 00 x11 | frame sequence
// number, as they reach the component after TcDeframer strips the primary header and
// Segment Header and CcsdsSdlsDeframer strips the SPI: IV (12) | ciphertext | MAC (16).
// Generated with the same independent implementation.
// ----------------------------------------------------------------------

const U16 E2E_SPI = 0x0001;

//! S1: VC 1, FSN 0x0A, SH 0x40 (FIRST, MAP 0), first 10 octets of a 20-octet Space Packet
const U8 E2E_S1_SH = 0x40;
const U8 E2E_S1_VC = 1;
const U8 E2E_S1_IV[AesGcmDecryptorTester::GCM_IV_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x0A};
const U8 E2E_S1_PLAINTEXT[10] = {0x18, 0x0F, 0xC0, 0x09, 0x00, 0x0D, 0x10, 0x11, 0x12, 0x13};
const U8 E2E_S1_CIPHERTEXT[10] = {0x7D, 0x19, 0xF9, 0x38, 0x5F, 0xEB, 0xC6, 0xDE, 0xF8, 0xEB};
const U8 E2E_S1_MAC[AesGcmDecryptorTester::GCM_TAG_LEN] = {0xE4, 0xF9, 0x83, 0x80, 0x47, 0xD7, 0xA8, 0x2F,
                                                           0x21, 0x77, 0x85, 0x1D, 0x13, 0x98, 0x3E, 0xDF};

//! S2: VC 1, FSN 0x0B, SH 0x80 (LAST, MAP 0), last 10 octets of the same Space Packet
const U8 E2E_S2_SH = 0x80;
const U8 E2E_S2_IV[AesGcmDecryptorTester::GCM_IV_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x0B};
const U8 E2E_S2_PLAINTEXT[10] = {0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D};
const U8 E2E_S2_CIPHERTEXT[10] = {0x8B, 0xD5, 0x1B, 0x18, 0x17, 0x9D, 0xC4, 0xF6, 0xCA, 0xC9};
const U8 E2E_S2_MAC[AesGcmDecryptorTester::GCM_TAG_LEN] = {0x0D, 0xB9, 0x34, 0x63, 0x93, 0xA9, 0xF9, 0xB7,
                                                           0x0A, 0xD2, 0x19, 0x37, 0x3E, 0x69, 0xA4, 0x16};

//! S2': S2 with the last MAC octet flipped 0x16 -> 0x17. On the link the FECF is recomputed
//! (6CD7 -> 7CF6) so the frame passes the TcDeframer CRC check and reaches this component.
const U8 E2E_S2_TAMPERED_MAC[AesGcmDecryptorTester::GCM_TAG_LEN] = {0x0D, 0xB9, 0x34, 0x63, 0x93, 0xA9, 0xF9, 0xB7,
                                                                    0x0A, 0xD2, 0x19, 0x37, 0x3E, 0x69, 0xA4, 0x17};

//! VC 0, FSN 0x0C, SH 0xC0 (UNSEGMENTED, MAP 0), a 12-octet Space Packet
const U8 E2E_VC0_SH = 0xC0;
const U8 E2E_VC0_VC = 0;
const U8 E2E_VC0_IV[AesGcmDecryptorTester::GCM_IV_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x0C};
const U8 E2E_VC0_PLAINTEXT[12] = {0x18, 0x0F, 0xC0, 0x08, 0x00, 0x05, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
const U8 E2E_VC0_CIPHERTEXT[12] = {0x6C, 0x5F, 0x2E, 0xD2, 0x92, 0x03, 0x8B, 0x71, 0x4F, 0x64, 0x2E, 0x63};
const U8 E2E_VC0_MAC[AesGcmDecryptorTester::GCM_TAG_LEN] = {0xD6, 0x5C, 0x2D, 0x0B, 0xC7, 0x75, 0x62, 0x1D,
                                                            0x58, 0x84, 0xC1, 0x71, 0x59, 0xB2, 0x6B, 0x49};

// ----------------------------------------------------------------------
// Independent reimplementations of what the component does
// ----------------------------------------------------------------------

//! Length of the AAD an SDLS-protected TC transfer frame authenticates: the 5-byte primary
//! header, the 2-byte SPI, and the 12-byte IV field
constexpr FwSizeType TC_AAD_LEN = 5 + 2 + AesGcmDecryptorTester::GCM_IV_LEN;

//! Build the TC additional authenticated data.
//! The primary header masked to 0xFC at byte 2 (the 6-bit VCID in bits
//! 7..2), the SPI, and a zeroed IV field.
void buildTcAad(U8 (&aad)[TC_AAD_LEN], U8 vcId, U16 spi) {
    (void)::memset(aad, 0, TC_AAD_LEN);
    aad[2] = static_cast<U8>((vcId << 2) & 0xFC);
    aad[5] = static_cast<U8>(spi >> 8);
    aad[6] = static_cast<U8>(spi & 0xFF);
}

//! Length of the AAD when the frame carries a TC Segment Header: one more octet, the
//! Segment Header itself, between the primary header and the SPI
constexpr FwSizeType TC_AAD_SH_LEN = TC_AAD_LEN + 1;

//! Build the TC additional authenticated data for a frame carrying a Segment Header:
//! the masked primary header, the Segment Header octet verbatim, the SPI, and a zeroed IV field.
void buildTcAadSh(U8 (&aad)[TC_AAD_SH_LEN], U8 vcId, U8 segmentHeader, U16 spi) {
    (void)::memset(aad, 0, TC_AAD_SH_LEN);
    aad[2] = static_cast<U8>((vcId << 2) & 0xFC);
    aad[5] = segmentHeader;
    aad[6] = static_cast<U8>(spi >> 8);
    aad[7] = static_cast<U8>(spi & 0xFF);
}

//! AES-256-GCM encrypt, used to manufacture frames for the component to decrypt
void gcmEncrypt(const U8* key,
                const U8* iv,
                const U8* aad,
                FwSizeType aadLen,
                const U8* plaintext,
                FwSizeType plainLen,
                U8* ciphertext,
                U8* tag) {
    EVP_CIPHER_CTX* const ctx = EVP_CIPHER_CTX_new();
    ASSERT_NE(ctx, nullptr);
    int len = 0;
    ASSERT_EQ(EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr), 1);
    ASSERT_EQ(
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(AesGcmDecryptorTester::GCM_IV_LEN), nullptr),
        1);
    ASSERT_EQ(EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, iv), 1);
    ASSERT_EQ(EVP_EncryptUpdate(ctx, nullptr, &len, aad, static_cast<int>(aadLen)), 1);
    if (plainLen > 0) {
        ASSERT_EQ(EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, static_cast<int>(plainLen)), 1);
    }
    int finalLen = 0;
    ASSERT_EQ(EVP_EncryptFinal_ex(ctx, ciphertext + plainLen, &finalLen), 1);
    ASSERT_EQ(finalLen, 0);
    ASSERT_EQ(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(AesGcmDecryptorTester::GCM_TAG_LEN), tag),
              1);
    EVP_CIPHER_CTX_free(ctx);
}

}  // namespace

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

AesGcmDecryptorTester ::AesGcmDecryptorTester()
    : AesGcmDecryptorGTestBase("AesGcmDecryptorTester", AesGcmDecryptorTester::MAX_HISTORY_SIZE),
      component("AesGcmDecryptor"),
      m_key(),
      m_keyLen(AES_256_KEY_LEN),
      m_keyStatus(Svc::Ccsds::SdlsStatus::SUCCESS),
      m_storage() {
    this->initComponents();
    this->connectPorts();
    this->setKey(KAT_KEY, AES_256_KEY_LEN, Svc::Ccsds::SdlsStatus::SUCCESS);
}

AesGcmDecryptorTester ::~AesGcmDecryptorTester() {}

// ----------------------------------------------------------------------
// Handler overrides
// ----------------------------------------------------------------------

Svc::Ccsds::SdlsStatus AesGcmDecryptorTester ::from_keyGet_handler(FwIndexType portNum,
                                                                   U16 securityAssociationIndex,
                                                                   Svc::Ccsds::SdlsKeyBuffer& key) {
    this->pushFromPortEntry_keyGet(securityAssociationIndex, key);
    if (this->m_keyStatus != Svc::Ccsds::SdlsStatus::SUCCESS) {
        const Fw::SerializeStatus status = key.setBuffLen(0);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));
        return this->m_keyStatus;
    }
    (void)::memcpy(key.getBuffAddr(), this->m_key, static_cast<size_t>(this->m_keyLen));
    const Fw::SerializeStatus status = key.setBuffLen(this->m_keyLen);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));
    return Svc::Ccsds::SdlsStatus::SUCCESS;
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void AesGcmDecryptorTester ::testKnownAnswer() {
    // The vector was generated for VC 5 / SPI 0x1234, which the fixture is configured for
    (void)::memcpy(this->m_storage, KAT_IV, GCM_IV_LEN);
    (void)::memcpy(this->m_storage + GCM_IV_LEN, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT);
    (void)::memcpy(this->m_storage + GCM_IV_LEN + sizeof KAT_CIPHERTEXT, KAT_MAC, GCM_TAG_LEN);
    Fw::Buffer frame(this->m_storage, GCM_IV_LEN + sizeof KAT_CIPHERTEXT + GCM_TAG_LEN);

    this->sendDecrypt(frame, TEST_SPI);

    this->assertPlaintext(KAT_PLAINTEXT, sizeof KAT_PLAINTEXT);
}

void AesGcmDecryptorTester ::testAuthMaskLayout() {
    // Check the ends of the VCID field as well as an ordinary value: a shift or mask
    // error shows up at the boundaries first
    const U8 vcIds[] = {0, 1, TEST_VC_ID, 0x3F};
    for (FwSizeType i = 0; i < FW_NUM_ARRAY_ELEMENTS(vcIds); i++) {
        const U16 spi = static_cast<U16>(STest::Pick::lowerUpper(0, 0xFFFF));
        U8 expected[TC_AAD_LEN];
        buildTcAad(expected, vcIds[i], spi);

        const Svc::Ccsds::Utils::SdlsTcAuthMask actual(vcIds[i], spi);

        ASSERT_EQ(actual.size, TC_AAD_LEN);
        ASSERT_EQ(::memcmp(actual.bytes, expected, TC_AAD_LEN), 0)
            << "TC auth mask does not match the ground segment's layout for VC " << static_cast<U32>(vcIds[i]);
    }
}

void AesGcmDecryptorTester ::testAuthMaskNoSh() {
    const U8 expected[TC_AAD_LEN] = {0x00, 0x00, 0x14, 0x00, 0x00, 0x12, 0x34, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    const Svc::Ccsds::Utils::SdlsTcAuthMask twoArg(TEST_VC_ID, TEST_SPI);
    ASSERT_EQ(twoArg.size, TC_AAD_LEN);
    ASSERT_EQ(::memcmp(twoArg.bytes, expected, TC_AAD_LEN), 0);

    // With no Segment Header present the octet argument must not reach the AAD
    const Svc::Ccsds::Utils::SdlsTcAuthMask fourArg(TEST_VC_ID, TEST_SPI, false, 0xFF);
    ASSERT_EQ(fourArg.size, TC_AAD_LEN);
    ASSERT_EQ(::memcmp(fourArg.bytes, expected, TC_AAD_LEN), 0);
    ASSERT_EQ(::memcmp(fourArg.bytes, twoArg.bytes, TC_AAD_LEN), 0);
}

void AesGcmDecryptorTester ::testAuthMaskSh() {
    const U8 expected[TC_AAD_SH_LEN] = {0x00, 0x00, 0x14, 0x00, 0x00, 0x41, 0x12, 0x34, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    const Svc::Ccsds::Utils::SdlsTcAuthMask actual(TEST_VC_ID, TEST_SPI, true, KAT_SH_FIRST);
    ASSERT_EQ(actual.size, TC_AAD_SH_LEN);
    ASSERT_EQ(::memcmp(actual.bytes, expected, TC_AAD_SH_LEN), 0);

    // Same layout as the harness's own construction across the VCID range and random SPIs
    const U8 vcIds[] = {0, 1, TEST_VC_ID, 0x3F};
    for (FwSizeType i = 0; i < FW_NUM_ARRAY_ELEMENTS(vcIds); i++) {
        const U16 spi = static_cast<U16>(STest::Pick::lowerUpper(0, 0xFFFF));
        const U8 sh = static_cast<U8>(STest::Pick::lowerUpper(0, 0xFF));
        U8 reference[TC_AAD_SH_LEN];
        buildTcAadSh(reference, vcIds[i], sh, spi);
        const Svc::Ccsds::Utils::SdlsTcAuthMask mask(vcIds[i], spi, true, sh);
        ASSERT_EQ(mask.size, TC_AAD_SH_LEN);
        ASSERT_EQ(::memcmp(mask.bytes, reference, TC_AAD_SH_LEN), 0)
            << "TC auth mask with Segment Header does not match for VC " << static_cast<U32>(vcIds[i]);
    }
}

void AesGcmDecryptorTester ::testDecryptVectorFirst() {
    Fw::Buffer frame = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC_FIRST);
    this->sendDecrypt(frame, TEST_SPI, TEST_VC_ID, true, KAT_SH_FIRST);
    this->assertPlaintext(KAT_PLAINTEXT, sizeof KAT_PLAINTEXT);
}

void AesGcmDecryptorTester ::testDecryptVectorUnsegmented() {
    Fw::Buffer frame = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC_UNSEGMENTED);
    this->sendDecrypt(frame, TEST_SPI, TEST_VC_ID, true, KAT_SH_UNSEGMENTED);
    this->assertPlaintext(KAT_PLAINTEXT, sizeof KAT_PLAINTEXT);
}

void AesGcmDecryptorTester ::testDecryptVectorContinuingLast() {
    Fw::Buffer continuing = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC_CONTINUING);
    this->sendDecrypt(continuing, TEST_SPI, TEST_VC_ID, true, KAT_SH_CONTINUING);
    this->assertPlaintext(KAT_PLAINTEXT, sizeof KAT_PLAINTEXT);

    Fw::Buffer last = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC_LAST);
    this->sendDecrypt(last, TEST_SPI, TEST_VC_ID, true, KAT_SH_LAST);
    this->assertPlaintext(KAT_PLAINTEXT, sizeof KAT_PLAINTEXT);
}

void AesGcmDecryptorTester ::testShNotInAadFails() {
    // The FIRST frame authenticates its Segment Header; a context claiming there was none
    // yields the 19-byte AAD, so the MAC no longer matches
    Fw::Buffer first = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC_FIRST);
    this->sendDecrypt(first, TEST_SPI, TEST_VC_ID, false, 0);
    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);

    // And the converse: the 19-byte frame presented as if it carried a Segment Header
    Fw::Buffer baseline = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC);
    this->sendDecrypt(baseline, TEST_SPI, TEST_VC_ID, true, KAT_SH_FIRST);
    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);
}

void AesGcmDecryptorTester ::testShTamperFails() {
    // FIRST on MAP 1 (0x41) presented as FIRST on MAP 2 (0x42): one bit in the Segment Header
    Fw::Buffer mapTamper = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC_FIRST);
    this->sendDecrypt(mapTamper, TEST_SPI, TEST_VC_ID, true, 0x42);
    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);

    // FIRST (0x41) presented as UNSEGMENTED (0xC1): the sequence flags
    Fw::Buffer flagsTamper = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC_FIRST);
    this->sendDecrypt(flagsTamper, TEST_SPI, TEST_VC_ID, true, KAT_SH_UNSEGMENTED);
    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);
}

void AesGcmDecryptorTester ::testAadLengthPassed() {
    // Same component instance: a 19-byte AAD frame then a 20-byte one. Had the whole backing
    // array been handed to the cipher, the 19-byte vector would carry one extra zero octet of
    // AAD and fail
    Fw::Buffer baseline = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC);
    this->sendDecrypt(baseline, TEST_SPI, TEST_VC_ID, false, 0);
    this->assertPlaintext(KAT_PLAINTEXT, sizeof KAT_PLAINTEXT);

    Fw::Buffer first = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC_FIRST);
    this->sendDecrypt(first, TEST_SPI, TEST_VC_ID, true, KAT_SH_FIRST);
    this->assertPlaintext(KAT_PLAINTEXT, sizeof KAT_PLAINTEXT);
}

void AesGcmDecryptorTester ::testDecryptFrameVc0() {
    Fw::Buffer frame = this->loadFrame(E2E_VC0_IV, E2E_VC0_CIPHERTEXT, sizeof E2E_VC0_CIPHERTEXT, E2E_VC0_MAC);
    this->sendDecrypt(frame, E2E_SPI, E2E_VC0_VC, true, E2E_VC0_SH);
    this->assertPlaintext(E2E_VC0_PLAINTEXT, sizeof E2E_VC0_PLAINTEXT);
}

void AesGcmDecryptorTester ::testDecryptFrameVc1() {
    Fw::Buffer s1 = this->loadFrame(E2E_S1_IV, E2E_S1_CIPHERTEXT, sizeof E2E_S1_CIPHERTEXT, E2E_S1_MAC);
    this->sendDecrypt(s1, E2E_SPI, E2E_S1_VC, true, E2E_S1_SH);
    this->assertPlaintext(E2E_S1_PLAINTEXT, sizeof E2E_S1_PLAINTEXT);

    Fw::Buffer s2 = this->loadFrame(E2E_S2_IV, E2E_S2_CIPHERTEXT, sizeof E2E_S2_CIPHERTEXT, E2E_S2_MAC);
    this->sendDecrypt(s2, E2E_SPI, E2E_S1_VC, true, E2E_S2_SH);
    this->assertPlaintext(E2E_S2_PLAINTEXT, sizeof E2E_S2_PLAINTEXT);

    // The VC byte of the AAD is authenticated: the same S1 frame under a VC 0 context fails
    Fw::Buffer wrongVc = this->loadFrame(E2E_S1_IV, E2E_S1_CIPHERTEXT, sizeof E2E_S1_CIPHERTEXT, E2E_S1_MAC);
    this->sendDecrypt(wrongVc, E2E_SPI, E2E_VC0_VC, true, E2E_S1_SH);
    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);
}

void AesGcmDecryptorTester ::testTamperedMacVector() {
    Fw::Buffer s2 = this->loadFrame(E2E_S2_IV, E2E_S2_CIPHERTEXT, sizeof E2E_S2_CIPHERTEXT, E2E_S2_TAMPERED_MAC);
    this->sendDecrypt(s2, E2E_SPI, E2E_S1_VC, true, E2E_S2_SH);
    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);
    // Nothing but a failure status leaves the component for this frame
    ASSERT_from_decryptOut_SIZE(1);
}

void AesGcmDecryptorTester ::testPerFrameAad() {
    // Alternate frames with and without a Segment Header, and with differing Segment Headers,
    // on one (VC, SPI) through one component instance: every frame must authenticate against
    // the AAD of that frame, not the AAD of the previous one
    U8 plaintext[24];
    const U8 headers[] = {KAT_SH_FIRST, KAT_SH_CONTINUING, KAT_SH_LAST, KAT_SH_UNSEGMENTED};
    for (FwSizeType i = 0; i < FW_NUM_ARRAY_ELEMENTS(headers); i++) {
        (void)::memset(plaintext, static_cast<int>(0x10 + i), sizeof plaintext);
        Fw::Buffer withSh = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID, TEST_SPI, true, headers[i]);
        this->sendDecrypt(withSh, TEST_SPI, TEST_VC_ID, true, headers[i]);
        this->assertPlaintext(plaintext, sizeof plaintext);

        (void)::memset(plaintext, static_cast<int>(0x20 + i), sizeof plaintext);
        Fw::Buffer withoutSh = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID, TEST_SPI);
        this->sendDecrypt(withoutSh, TEST_SPI);
        this->assertPlaintext(plaintext, sizeof plaintext);
    }

    // Then the fixed vectors in the same instance, interleaved the same way
    Fw::Buffer first = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC_FIRST);
    this->sendDecrypt(first, TEST_SPI, TEST_VC_ID, true, KAT_SH_FIRST);
    this->assertPlaintext(KAT_PLAINTEXT, sizeof KAT_PLAINTEXT);

    Fw::Buffer baseline = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC);
    this->sendDecrypt(baseline, TEST_SPI);
    this->assertPlaintext(KAT_PLAINTEXT, sizeof KAT_PLAINTEXT);

    Fw::Buffer last = this->loadFrame(KAT_IV, KAT_CIPHERTEXT, sizeof KAT_CIPHERTEXT, KAT_MAC_LAST);
    this->sendDecrypt(last, TEST_SPI, TEST_VC_ID, true, KAT_SH_LAST);
    this->assertPlaintext(KAT_PLAINTEXT, sizeof KAT_PLAINTEXT);
}

void AesGcmDecryptorTester ::testDecryptNominal() {
    U8 plaintext[64];
    for (FwSizeType i = 0; i < sizeof plaintext; i++) {
        plaintext[i] = static_cast<U8>(STest::Pick::lowerUpper(0, 0xFF));
    }
    Fw::Buffer frame = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID, TEST_SPI);

    this->sendDecrypt(frame, TEST_SPI);

    this->assertPlaintext(plaintext, sizeof plaintext);
    // Decryption is in place: the plaintext starts where the ciphertext was, one IV in
    const Fw::Buffer out = this->fromPortHistory_decryptOut->at(0).data;
    ASSERT_EQ(out.getData(), this->m_storage + GCM_IV_LEN);
}

void AesGcmDecryptorTester ::testEmptyCiphertext() {
    // A frame of exactly IV + MAC carries no ciphertext, but is still authenticated
    Fw::Buffer frame = this->buildFrame(nullptr, 0, TEST_VC_ID, TEST_SPI);
    ASSERT_EQ(frame.getSize(), GCM_IV_LEN + GCM_TAG_LEN);

    this->sendDecrypt(frame, TEST_SPI);

    this->assertStatus(Svc::Ccsds::SdlsStatus::SUCCESS);
    ASSERT_EQ(this->fromPortHistory_decryptOut->at(0).data.getSize(), static_cast<FwSizeType>(0));
}

void AesGcmDecryptorTester ::testAllocationContextPreserved() {
    // A real frame arrives from a Svc.BufferManager, which deallocates by context and
    // asserts the data pointer lies inside the slot it handed out
    const U32 context = static_cast<U32>(STest::Pick::lowerUpper(0, 0xFFFE));
    U8 plaintext[32];
    (void)::memset(plaintext, 0xA5, sizeof plaintext);
    Fw::Buffer frame = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID, TEST_SPI);
    Fw::Buffer withContext(frame.getData(), frame.getSize(), context);

    this->sendDecrypt(withContext, TEST_SPI);

    this->assertStatus(Svc::Ccsds::SdlsStatus::SUCCESS);
    const Fw::Buffer out = this->fromPortHistory_decryptOut->at(0).data;
    ASSERT_EQ(out.getContext(), context);
    ASSERT_EQ(out.getOriginalData(), this->m_storage);
}

void AesGcmDecryptorTester ::testTamperedCiphertext() {
    U8 plaintext[48];
    (void)::memset(plaintext, 0x5A, sizeof plaintext);
    Fw::Buffer frame = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID, TEST_SPI);
    // Flip one bit somewhere in the ciphertext
    const U32 offset = STest::Pick::lowerUpper(0, static_cast<U32>(sizeof plaintext) - 1);
    const FwSizeType index = GCM_IV_LEN + static_cast<FwSizeType>(offset);
    this->m_storage[index] ^= 0x01;

    this->sendDecrypt(frame, TEST_SPI);

    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);
}

void AesGcmDecryptorTester ::testTamperedMac() {
    U8 plaintext[48];
    (void)::memset(plaintext, 0x5A, sizeof plaintext);
    Fw::Buffer frame = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID, TEST_SPI);
    const FwSizeType macStart = GCM_IV_LEN + sizeof plaintext;
    const U32 offset = STest::Pick::lowerUpper(0, static_cast<U32>(GCM_TAG_LEN) - 1);
    const FwSizeType index = macStart + static_cast<FwSizeType>(offset);
    this->m_storage[index] ^= 0x01;

    this->sendDecrypt(frame, TEST_SPI);

    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);
}

void AesGcmDecryptorTester ::testTamperedIv() {
    U8 plaintext[48];
    (void)::memset(plaintext, 0x5A, sizeof plaintext);
    Fw::Buffer frame = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID, TEST_SPI);
    const FwSizeType index = static_cast<FwSizeType>(STest::Pick::lowerUpper(0, static_cast<U32>(GCM_IV_LEN) - 1));
    this->m_storage[index] ^= 0x01;

    this->sendDecrypt(frame, TEST_SPI);

    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);
}

void AesGcmDecryptorTester ::testWrongVcId() {
    U8 plaintext[32];
    (void)::memset(plaintext, 0x11, sizeof plaintext);
    // Authenticated for a channel other than the one the component is configured for
    Fw::Buffer frame = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID + 1, TEST_SPI);

    this->sendDecrypt(frame, TEST_SPI);

    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);
}

void AesGcmDecryptorTester ::testWrongSecurityAssociation() {
    U8 plaintext[32];
    (void)::memset(plaintext, 0x22, sizeof plaintext);
    Fw::Buffer frame = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID, TEST_SPI);

    // Same frame, presented under a different SA
    this->sendDecrypt(frame, static_cast<U16>(TEST_SPI + 1));

    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);
}

void AesGcmDecryptorTester ::testVcFromContext() {
    const U8 otherVcId = TEST_VC_ID + 1;

    U8 plaintext[32];
    (void)::memset(plaintext, 0x66, sizeof plaintext);
    // A frame authenticated for another VC decrypts when the context names that VC
    Fw::Buffer frame = this->buildFrame(plaintext, sizeof plaintext, otherVcId, TEST_SPI);
    this->sendDecrypt(frame, TEST_SPI, otherVcId);
    this->assertPlaintext(plaintext, sizeof plaintext);

    // The same frame under a context naming a different VC no longer authenticates
    Fw::Buffer stale = this->buildFrame(plaintext, sizeof plaintext, otherVcId, TEST_SPI);
    this->sendDecrypt(stale, TEST_SPI, TEST_VC_ID);
    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);
}

void AesGcmDecryptorTester ::testRecoversAfterMacFailure() {
    U8 plaintext[40];
    (void)::memset(plaintext, 0x77, sizeof plaintext);

    // Reject one frame on its MAC
    Fw::Buffer bad = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID, TEST_SPI);
    this->m_storage[GCM_IV_LEN] ^= 0x01;
    this->sendDecrypt(bad, TEST_SPI);
    this->assertStatus(Svc::Ccsds::SdlsStatus::MAC_VERIFICATION_FAILURE);

    // The next good frame must still decrypt
    Fw::Buffer good = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID, TEST_SPI);
    this->sendDecrypt(good, TEST_SPI);
    this->assertPlaintext(plaintext, sizeof plaintext);
}

void AesGcmDecryptorTester ::testShortBuffer() {
    // One byte below the smallest well-formed frame
    Fw::Buffer frame(this->m_storage, GCM_IV_LEN + GCM_TAG_LEN - 1);

    this->sendDecrypt(frame, TEST_SPI);

    this->assertStatus(Svc::Ccsds::SdlsStatus::DECRYPTION_FAILURE);
    // Rejected on shape alone, before the key is ever fetched
    ASSERT_from_keyGet_SIZE(0);
}

void AesGcmDecryptorTester ::testKeyUnavailable() {
    U8 plaintext[32];
    (void)::memset(plaintext, 0x33, sizeof plaintext);
    Fw::Buffer frame = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID, TEST_SPI);
    this->setKey(nullptr, 0, Svc::Ccsds::SdlsStatus::KEY_ERROR);

    this->sendDecrypt(frame, TEST_SPI);

    this->assertStatus(Svc::Ccsds::SdlsStatus::KEY_ERROR);
}

void AesGcmDecryptorTester ::testWrongKeySize() {
    U8 plaintext[32];
    (void)::memset(plaintext, 0x44, sizeof plaintext);
    Fw::Buffer frame = this->buildFrame(plaintext, sizeof plaintext, TEST_VC_ID, TEST_SPI);
    // An AES-128 key: long enough to use, and silently wrong if it were used
    this->setKey(KAT_KEY, 16, Svc::Ccsds::SdlsStatus::SUCCESS);

    this->sendDecrypt(frame, TEST_SPI);

    this->assertStatus(Svc::Ccsds::SdlsStatus::KEY_ERROR);
}

void AesGcmDecryptorTester ::testBufferReturn() {
    this->clearHistory();
    const U32 size = STest::Pick::lowerUpper(1, static_cast<U32>(TEST_BUFFER_SIZE));
    Fw::Buffer buffer(this->m_storage, static_cast<FwSizeType>(size));
    ComCfg::FrameContext context;
    context.set_vcId(static_cast<U8>(STest::Pick::lowerUpper(0, 7)));

    this->invoke_to_decryptReturnIn(0, buffer, context);

    ASSERT_from_bufferReturnOut_SIZE(1);
    ASSERT_from_bufferReturnOut(0, buffer, context);
    ASSERT_from_decryptOut_SIZE(0);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void AesGcmDecryptorTester ::setKey(const U8* key, FwSizeType keyLen, Svc::Ccsds::SdlsStatus status) {
    FW_ASSERT(keyLen <= AES_256_KEY_LEN, static_cast<FwAssertArgType>(keyLen));
    (void)::memset(this->m_key, 0, sizeof this->m_key);
    if (key != nullptr) {
        (void)::memcpy(this->m_key, key, static_cast<size_t>(keyLen));
    }
    this->m_keyLen = keyLen;
    this->m_keyStatus = status;
}

Fw::Buffer AesGcmDecryptorTester ::buildFrame(const U8* plaintext,
                                              FwSizeType plainLen,
                                              U8 vcId,
                                              U16 spi,
                                              bool segmentHeaderPresent,
                                              U8 segmentHeader) {
    const FwSizeType frameLen = GCM_IV_LEN + plainLen + GCM_TAG_LEN;
    FW_ASSERT(frameLen <= TEST_BUFFER_SIZE, static_cast<FwAssertArgType>(frameLen));

    for (FwSizeType i = 0; i < GCM_IV_LEN; i++) {
        this->m_storage[i] = static_cast<U8>(STest::Pick::lowerUpper(0, 0xFF));
    }
    if (segmentHeaderPresent) {
        U8 aad[TC_AAD_SH_LEN];
        buildTcAadSh(aad, vcId, segmentHeader, spi);
        gcmEncrypt(this->m_key, this->m_storage, aad, TC_AAD_SH_LEN, plaintext, plainLen, this->m_storage + GCM_IV_LEN,
                   this->m_storage + GCM_IV_LEN + plainLen);
    } else {
        U8 aad[TC_AAD_LEN];
        buildTcAad(aad, vcId, spi);
        gcmEncrypt(this->m_key, this->m_storage, aad, TC_AAD_LEN, plaintext, plainLen, this->m_storage + GCM_IV_LEN,
                   this->m_storage + GCM_IV_LEN + plainLen);
    }
    return Fw::Buffer(this->m_storage, frameLen);
}

Fw::Buffer AesGcmDecryptorTester ::loadFrame(const U8* iv, const U8* ciphertext, FwSizeType cipherLen, const U8* mac) {
    const FwSizeType frameLen = GCM_IV_LEN + cipherLen + GCM_TAG_LEN;
    FW_ASSERT(frameLen <= TEST_BUFFER_SIZE, static_cast<FwAssertArgType>(frameLen));
    (void)::memcpy(this->m_storage, iv, GCM_IV_LEN);
    (void)::memcpy(this->m_storage + GCM_IV_LEN, ciphertext, static_cast<size_t>(cipherLen));
    (void)::memcpy(this->m_storage + GCM_IV_LEN + cipherLen, mac, GCM_TAG_LEN);
    return Fw::Buffer(this->m_storage, frameLen);
}

void AesGcmDecryptorTester ::sendDecrypt(Fw::Buffer& data,
                                         U16 spi,
                                         U8 vcId,
                                         bool segmentHeaderPresent,
                                         U8 segmentHeader) {
    this->clearHistory();
    ComCfg::FrameContext context;
    // The VC and the stripped Segment Header reach the component on the context, as
    // TcDeframer sets them upstream
    context.set_vcId(vcId);
    context.set_tcSegmentHeaderPresent(segmentHeaderPresent);
    context.set_tcSegmentHeader(segmentHeader);
    this->invoke_to_decryptIn(0, spi, data, context);
}

void AesGcmDecryptorTester ::assertStatus(Svc::Ccsds::SdlsStatus status) {
    // Every path through decryptIn ends in exactly one decryptOut, whatever the outcome
    ASSERT_from_decryptOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_decryptOut->at(0).status, status);
}

void AesGcmDecryptorTester ::assertPlaintext(const U8* expected, FwSizeType expectedLen) {
    this->assertStatus(Svc::Ccsds::SdlsStatus::SUCCESS);
    const Fw::Buffer out = this->fromPortHistory_decryptOut->at(0).data;
    ASSERT_EQ(out.getSize(), expectedLen);
    ASSERT_EQ(::memcmp(out.getData(), expected, static_cast<size_t>(expectedLen)), 0);
}

}  // namespace Ccsds

}  // namespace Svc
