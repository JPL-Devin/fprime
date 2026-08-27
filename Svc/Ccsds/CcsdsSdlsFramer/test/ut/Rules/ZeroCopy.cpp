// ======================================================================
// \title  ZeroCopy.cpp
// \author devin
// \brief  Rule implementations for the ZeroCopy rule group
//
// These rules exercise the zero-copy zone path: a packer-built zone with
// headroom is encrypted in place and framed without allocation, or falls
// back to allocate-and-copy when the encryptor returns a different buffer.
// ======================================================================

#include <cstring>

#include "STest/Pick/Pick.hpp"
#include "Svc/Ccsds/CcsdsSdlsFramer/test/ut/CcsdsSdlsFramerTester.hpp"

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// ZeroCopy.InPlace
// ----------------------------------------------------------------------

bool CcsdsSdlsFramerTester::ZeroCopy__InPlace__precondition() const {
    return true;
}

void CcsdsSdlsFramerTester::ZeroCopy__InPlace__action() {
    this->clearHistory();

    // Build a zone buffer with headroom, as SppZonePacker would
    const FwSizeType headroom = 4;
    const FwSizeType zoneSize = TEST_BUFFER_SIZE - 2 * headroom;
    U8 backer[TEST_BUFFER_SIZE];
    for (FwSizeType i = 0; i < sizeof backer; i++) {
        backer[i] = static_cast<U8>(STest::Pick::lowerUpper(0, 0xFF));
    }
    Fw::Buffer zone(backer, sizeof backer);
    zone.advance(static_cast<FwSignedSizeType>(headroom));
    zone.setSize(static_cast<Fw::Buffer::SizeType>(zoneSize));

    // Exclude the SaIndexUnset sentinel: it would make the component substitute the SA_INDEX parameter
    const U16 sa = static_cast<U16>(STest::Pick::lowerUpper(0, ComCfg::SaIndexUnset - 1));
    ComCfg::FrameContext context;
    context.set_saIndex(sa);
    context.set_zeroCopyFrame(true);

    this->invoke_to_dataIn(0, zone, context);
    ASSERT_from_encryptOut_SIZE(1);
    const FromPortEntry_encryptOut& encryptEntry = this->fromPortHistory_encryptOut->at(0);

    // The encryptor encrypted in place: same buffer comes back on encryptIn
    Fw::Buffer encrypted = encryptEntry.data;
    ComCfg::FrameContext encryptedContext = encryptEntry.context;
    this->invoke_to_encryptIn(0, Svc::Ccsds::SdlsStatus::SUCCESS, encrypted, encryptedContext);

    // No allocation and no encryptor return: the zone is framed in place with the SA
    // index prepended into the headroom
    ASSERT_from_bufferAllocate_SIZE(0);
    ASSERT_from_encryptReturnOut_SIZE(0);
    ASSERT_from_dataOut_SIZE(1);
    const FromPortEntry_dataOut& frameEntry = this->fromPortHistory_dataOut->at(0);
    ASSERT_EQ(frameEntry.data.getData(), &backer[headroom - sizeof(U16)]);
    ASSERT_EQ(frameEntry.data.getSize(), zoneSize + sizeof(U16));
    ASSERT_EQ(frameEntry.data.getData()[0], static_cast<U8>(sa >> 8));
    ASSERT_EQ(frameEntry.data.getData()[1], static_cast<U8>(sa & 0xFF));
    ASSERT_TRUE(frameEntry.context.get_zeroCopyFrame());

    // Returning the frame forwards ownership upstream instead of deallocating. The downstream
    // framer returns the buffer advanced to the frame start (a different window offset), so the
    // return must be matched by backing region, not by exact data pointer
    Fw::Buffer frame = frameEntry.data;
    frame.advance(-static_cast<FwSignedSizeType>(headroom - sizeof(U16)));
    frame.setSize(static_cast<Fw::Buffer::SizeType>(TEST_BUFFER_SIZE));
    this->invoke_to_dataReturnIn(0, frame, frameEntry.context);
    ASSERT_from_bufferDeallocate_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), frame.getData());
}

// ----------------------------------------------------------------------
// ZeroCopy.Fallback
// ----------------------------------------------------------------------

bool CcsdsSdlsFramerTester::ZeroCopy__Fallback__precondition() const {
    return true;
}

void CcsdsSdlsFramerTester::ZeroCopy__Fallback__action() {
    this->clearHistory();

    // A zero-copy zone goes out for encryption...
    const FwSizeType headroom = 4;
    const FwSizeType zoneSize = 16;
    U8 backer[TEST_BUFFER_SIZE];
    Fw::Buffer zone(backer, sizeof backer);
    zone.advance(static_cast<FwSignedSizeType>(headroom));
    zone.setSize(static_cast<Fw::Buffer::SizeType>(zoneSize));

    // Exclude the SaIndexUnset sentinel: it would make the component substitute the SA_INDEX parameter
    const U16 sa = static_cast<U16>(STest::Pick::lowerUpper(0, ComCfg::SaIndexUnset - 1));
    ComCfg::FrameContext context;
    context.set_saIndex(sa);
    context.set_zeroCopyFrame(true);

    this->invoke_to_dataIn(0, zone, context);
    ASSERT_from_encryptOut_SIZE(1);
    ComCfg::FrameContext encryptedContext = this->fromPortHistory_encryptOut->at(0).context;

    // ...but the encryptor returns a different buffer: the in-place contract is not met
    U8 encryptedStorage[zoneSize];
    for (FwSizeType i = 0; i < sizeof encryptedStorage; i++) {
        encryptedStorage[i] = static_cast<U8>(STest::Pick::lowerUpper(0, 0xFF));
    }
    Fw::Buffer encrypted(encryptedStorage, sizeof encryptedStorage);
    this->m_allocateInvalid = false;
    this->m_allocateUndersized = false;
    this->invoke_to_encryptIn(0, Svc::Ccsds::SdlsStatus::SUCCESS, encrypted, encryptedContext);

    // Fallback: allocate-and-copy with the SA index prepended, encrypted buffer returned
    ASSERT_from_bufferAllocate_SIZE(1);
    ASSERT_from_encryptReturnOut_SIZE(1);
    ASSERT_from_dataOut_SIZE(1);
    const FromPortEntry_dataOut& frameEntry = this->fromPortHistory_dataOut->at(0);
    ASSERT_EQ(frameEntry.data.getSize(), zoneSize + sizeof(U16));
    ASSERT_EQ(frameEntry.data.getData()[0], static_cast<U8>(sa >> 8));
    ASSERT_EQ(frameEntry.data.getData()[1], static_cast<U8>(sa & 0xFF));
    ASSERT_EQ(::memcmp(&frameEntry.data.getData()[sizeof(U16)], encryptedStorage, zoneSize), 0);
    ASSERT_FALSE(frameEntry.context.get_zeroCopyFrame());

    // Returning the allocated frame deallocates it
    Fw::Buffer frame = frameEntry.data;
    this->invoke_to_dataReturnIn(0, frame, frameEntry.context);
    ASSERT_from_bufferDeallocate_SIZE(1);
    ASSERT_from_dataReturnOut_SIZE(0);
}

}  // namespace Ccsds

}  // namespace Svc
