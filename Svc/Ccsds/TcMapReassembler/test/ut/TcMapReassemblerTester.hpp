// ======================================================================
// \title  TcMapReassemblerTester.hpp
// \author thomas-bc-autobot
// \brief  hpp file for TcMapReassembler component test harness implementation class
// ======================================================================

#ifndef Svc_Ccsds_TcMapReassemblerTester_HPP
#define Svc_Ccsds_TcMapReassemblerTester_HPP

#include <vector>

#include "Fw/Types/MallocAllocator.hpp"
#include "Svc/BufferManager/BufferManager.hpp"
#include "Svc/Ccsds/TcMapReassembler/TcMapReassembler.hpp"
#include "Svc/Ccsds/TcMapReassembler/TcMapReassemblerGTestBase.hpp"
#include "Svc/Ccsds/TcMapReassembler/test/ut/TestState/TestState.hpp"
#include "TestUtils/RuleBasedTesting.hpp"

namespace Svc {

namespace Ccsds {

class TcMapReassemblerTester : public TcMapReassemblerGTestBase, public TcMapReassemblerTestState {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    //! Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 256;

    //! Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Memory identifier handed to the pool allocator
    static const FwEnumStoreType POOL_MEM_ID = 0;

    //! Test virtual channel carried in every context
    static const U8 TEST_VC_ID = 1;

    //! How the allocate proxy answers the component
    enum AllocMode {
        ALLOC_POOL,     //!< forward to the real Svc::BufferManager
        ALLOC_INVALID,  //!< return an invalid buffer (pool exhausted stub)
        ALLOC_SHORT     //!< return a valid buffer of requested - 1 octets (MF6)
    };

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object TcMapReassemblerTester
    TcMapReassemblerTester();

    //! Destroy object TcMapReassemblerTester
    ~TcMapReassemblerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests (DESIGN §8.3)
    // ----------------------------------------------------------------------

    void testUnsegmented();
    void testFirstLast();
    void testFirstContLast();
    void testShAbsent();
    void testInvalidMapId();
    void testOrphanContinuing();
    void testOrphanLast();
    void testEmptySegment();
    void testDuplicateFirst();
    void testUnsegmentedWhileInProgress();
    void testOverflowSegment();
    void testOverflowAccumulated();
    void testFirstDeclaredTooLarge();
    void testUnsegmentedDeclaredTooLarge();
    void testAllocFailInvalid();
    void testAllocFailShort();
    void testLengthMismatchLast();
    void testLengthMismatchUnsegmented();
    void testTooShortForHeader();
    void testMultiMap();
    void testInFlightBound();
    void testReturnDeallocates();
    void testMacFailureMidPacketModel();
    void testConfigureAsserts();
    void testSerializedSize();

  public:
    // ----------------------------------------------------------------------
    // Rules (DESIGN §8.4)
    // ----------------------------------------------------------------------

    FW_RBT_DEFINE_RULE(TcMapReassemblerTester, Segments, SendFirst);
    FW_RBT_DEFINE_RULE(TcMapReassemblerTester, Segments, SendContinuing);
    FW_RBT_DEFINE_RULE(TcMapReassemblerTester, Segments, SendLast);
    FW_RBT_DEFINE_RULE(TcMapReassemblerTester, Segments, SendUnsegmented);
    FW_RBT_DEFINE_RULE(TcMapReassemblerTester, Segments, SendOrphan);
    FW_RBT_DEFINE_RULE(TcMapReassemblerTester, Segments, SendInvalidMap);
    FW_RBT_DEFINE_RULE(TcMapReassemblerTester, Segments, SendEmpty);
    FW_RBT_DEFINE_RULE(TcMapReassemblerTester, Segments, SendOversize);
    FW_RBT_DEFINE_RULE(TcMapReassemblerTester, Segments, SendShAbsent);
    FW_RBT_DEFINE_RULE(TcMapReassemblerTester, Pool, ReturnPacket);
    FW_RBT_DEFINE_RULE(TcMapReassemblerTester, Pool, ExhaustPool);

    //! Invariants 1-7 of DESIGN §8.4, checked after every rule
    void checkInvariants();

  private:
    // ----------------------------------------------------------------------
    // Handler overrides for typed from ports
    // ----------------------------------------------------------------------

    //! Proxy to the real pool, with fault injection and accounting
    Fw::Buffer from_allocate_handler(FwIndexType portNum, FwSizeType size) override;

    //! Proxy to the real pool, with accounting
    void from_deallocate_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) override;

    //! Counts frame returns across history clears (invariant 4)
    void from_dataReturnOut_handler(FwIndexType portNum,
                                    Fw::Buffer& data,
                                    const ComCfg::FrameContext& context) override;

  private:
    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Configure the component with the test MAP-ID table
    void configureDefault();

    //! MAP ID of configured slot i
    static U8 testMapId(FwSizeType i);

    //! Context carrying a Segment Header for (flags, mapId)
    static ComCfg::FrameContext makeContext(TcSequenceFlags::T flags, U8 mapId, bool present = true);

    //! Fill pkt with len random octets; when len >= SP_HEADER_SIZE the header declares `declared` total octets
    static void buildPacket(std::vector<U8>& pkt, FwSizeType len, FwSizeType declared);

    //! Fill pkt with a well-formed Space Packet of len octets (declared == len)
    static void buildPacket(std::vector<U8>& pkt, FwSizeType len);

    //! Send one segment: clears the histories, invokes dataIn, asserts exactly one synchronous frame return
    void sendSegment(U8 mapId, TcSequenceFlags::T flags, const U8* data, FwSizeType len, bool present = true);

    //! Send the portion [offset, offset + len) of pkt
    void sendPortion(U8 mapId, TcSequenceFlags::T flags, const std::vector<U8>& pkt, FwSizeType offset, FwSizeType len);

    //! Return the delivered packet at dataOut history index through dataReturnIn
    void returnDelivered(FwSizeType index);

    //! Assert the component MAP is IDLE with no buffer
    void assertIdle(U8 mapId);

    //! Assert the component MAP is IN_PROGRESS holding exactly `received` octets
    void assertInProgress(U8 mapId, FwSizeType received);

    //! Assert the dataOut history entry equals pkt
    void assertDelivered(FwSizeType index, const std::vector<U8>& pkt);

    //! Assert exactly one errorNotify with the given value
    void assertError(FrameError::T err);

    //! Assert the component counters
    void assertCounters(U32 reassembled, U32 dropped, U32 abandoned);

    //! Take a buffer straight from the pool (bypassing the component) and hold it
    void holdPoolBuffer();

    //! Give every held buffer back to the pool
    void releaseHeldBuffers();

    //! Rule helper: predict the outcome from the shadow, send the segment, compare, update the shadow
    void ruleSend(U8 mapId, TcSequenceFlags::T flags, const U8* data, FwSizeType len, bool present = true);

    //! Rule helper: the pool has no free buffer (real pool: in progress + delivered + held)
    bool poolFull() const;

    //! Rule helper: flags value picked at random
    static TcSequenceFlags::T randomFlags();

    //! Rule helper: random octets of the given length into the scratch segment storage
    const U8* randomSegment(FwSizeType len);

    //! Rule helper: random well-formed Space Packet (declared == len) of len octets into map.planned
    static void planPacket(MapShadow& map, FwSizeType len);

    //! Declared Space Packet length of the first octets (0 when fewer than a header)
    static FwSizeType declaredOf(const std::vector<U8>& bytes);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    TcMapReassembler component;

    //! The real pool the component allocates from
    Svc::BufferManager m_pool;

    //! Allocator behind the pool; must outlive m_pool
    Fw::MallocAllocator m_allocator;

    //! Frame storage handed to dataIn (copy-always: reusable after every call)
    std::vector<U8> m_frame;

    //! Storage for the short-allocation stub
    std::vector<U8> m_short;

    //! Scratch segment storage for the rules
    std::vector<U8> m_scratch;

    //! Number of dataReturnOut calls since construction (invariant 4)
    U32 m_frameReturns = 0;

    //! Buffers held by the tester directly from the pool (ExhaustPool)
    std::vector<Fw::Buffer> m_held;

    //! Fault injection mode of the allocate proxy
    AllocMode m_allocMode = ALLOC_POOL;

    //! Number of pool buffers currently allocated (mirrors BufferManager currBuffs)
    U32 m_poolAllocated = 0;

    //! Highest m_poolAllocated seen (mirrors BufferManager highWater)
    U32 m_poolHighWater = 0;

    //! Number of short stub buffers handed out and not yet deallocated
    U32 m_shortOutstanding = 0;

    //! Total number of deallocate calls carrying the short stub buffer
    U32 m_shortDeallocated = 0;
};

}  // namespace Ccsds

}  // namespace Svc

#endif
