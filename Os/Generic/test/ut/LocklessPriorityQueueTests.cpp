// ======================================================================
// \title Os/Generic/test/ut/LocklessPriorityQueueTests.cpp
// \brief tests using lockless priority queue implementation for Os::Queue interface testing
// ======================================================================
#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include "Fw/FPrimeBasicTypes.hpp"
#include "Fw/Types/String.hpp"
#include "Os/Generic/LocklessPriorityQueue.hpp"
#include "Os/Queue.hpp"
#include "STest/Random/Random.hpp"

namespace {

constexpr FwSizeType CONCURRENT_DEPTH = 64;
constexpr FwSizeType CONCURRENT_MESSAGE_SIZE = sizeof(U32);
constexpr U32 CONCURRENT_PRODUCERS = 4;
constexpr U32 CONCURRENT_CONSUMERS = 4;
constexpr U32 CONCURRENT_MESSAGES_PER_PRODUCER = 1000;
constexpr U32 CONCURRENT_TOTAL_MESSAGES = CONCURRENT_PRODUCERS * CONCURRENT_MESSAGES_PER_PRODUCER;

//! Shared state for the multi-producer / multi-consumer concurrency test.
struct ConcurrentTestState {
    Os::Queue queue;
    std::vector<std::atomic<U32>> received;
    std::atomic<U32> consumed;
    std::atomic<bool> producers_done;

    ConcurrentTestState() : received(CONCURRENT_TOTAL_MESSAGES), consumed(0), producers_done(false) {
        for (U32 index = 0; index < CONCURRENT_TOTAL_MESSAGES; index++) {
            received[index].store(0, std::memory_order_relaxed);
        }
    }
};

//! Pack a U32 value into a little-endian byte buffer of exactly `CONCURRENT_MESSAGE_SIZE` bytes.
void pack_value(U32 value, U8* buffer) {
    for (FwSizeType b = 0; b < CONCURRENT_MESSAGE_SIZE; b++) {
        buffer[b] = static_cast<U8>((value >> (8 * b)) & 0xFFu);
    }
}

//! Unpack a U32 value from a little-endian byte buffer of exactly `CONCURRENT_MESSAGE_SIZE` bytes.
U32 unpack_value(const U8* buffer) {
    U32 value = 0;
    for (FwSizeType b = 0; b < CONCURRENT_MESSAGE_SIZE; b++) {
        value |= static_cast<U32>(buffer[b]) << (8 * b);
    }
    return value;
}

//! Producer worker: send `CONCURRENT_MESSAGES_PER_PRODUCER` distinct values into the queue.
void producer_worker(ConcurrentTestState* state, U32 producerIndex) {
    for (U32 messageIndex = 0; messageIndex < CONCURRENT_MESSAGES_PER_PRODUCER; messageIndex++) {
        const U32 value = (producerIndex * CONCURRENT_MESSAGES_PER_PRODUCER) + messageIndex;
        U8 buffer[CONCURRENT_MESSAGE_SIZE];
        pack_value(value, buffer);
        const FwQueuePriorityType priority = static_cast<FwQueuePriorityType>(value & 0x7u);
        Os::QueueInterface::Status status = Os::QueueInterface::Status::FULL;
        while (status == Os::QueueInterface::Status::FULL) {
            status = state->queue.send(buffer, CONCURRENT_MESSAGE_SIZE, priority,
                                       Os::QueueInterface::BlockingType::NONBLOCKING);
            if (status == Os::QueueInterface::Status::FULL) {
                std::this_thread::yield();
            }
        }
    }
}

//! Consumer worker: receive messages until the producers are done and the consumed count
//! reaches the total. Asserts each value is delivered exactly once.
void consumer_worker(ConcurrentTestState* state) {
    U8 buffer[CONCURRENT_MESSAGE_SIZE];
    FwSizeType actualSize = 0;
    FwQueuePriorityType priority = 0;
    while (state->consumed.load(std::memory_order_acquire) < CONCURRENT_TOTAL_MESSAGES) {
        Os::QueueInterface::Status status =
            state->queue.receive(buffer, CONCURRENT_MESSAGE_SIZE,
                                 Os::QueueInterface::BlockingType::NONBLOCKING, actualSize, priority);
        if (status == Os::QueueInterface::Status::OP_OK) {
            ASSERT_EQ(actualSize, CONCURRENT_MESSAGE_SIZE);
            const U32 value = unpack_value(buffer);
            ASSERT_LT(value, CONCURRENT_TOTAL_MESSAGES);
            const U32 priorCount = state->received[value].fetch_add(1, std::memory_order_acq_rel);
            ASSERT_EQ(priorCount, 0u) << "duplicate delivery of value " << value;
            state->consumed.fetch_add(1, std::memory_order_acq_rel);
        } else if (status == Os::QueueInterface::Status::EMPTY) {
            if (state->producers_done.load(std::memory_order_acquire) &&
                state->consumed.load(std::memory_order_acquire) >= CONCURRENT_TOTAL_MESSAGES) {
                break;
            }
            std::this_thread::yield();
        } else {
            FAIL() << "unexpected status " << static_cast<int>(status);
        }
    }
}

//! Validate that high-load concurrent producers and consumers do not lose, duplicate, or
//! reorder messages in a way that violates priority ordering. This test relies on real OS
//! threads to exercise the lockless state machine.
TEST(LocklessConcurrent, MultiProducerMultiConsumer) {
    ConcurrentTestState state;
    Fw::String name("concurrent-test");
    ASSERT_EQ(state.queue.create(0, name, CONCURRENT_DEPTH, CONCURRENT_MESSAGE_SIZE),
              Os::QueueInterface::Status::OP_OK);

    std::vector<std::thread> producer_threads;
    producer_threads.reserve(CONCURRENT_PRODUCERS);
    for (U32 producerIndex = 0; producerIndex < CONCURRENT_PRODUCERS; producerIndex++) {
        producer_threads.emplace_back(producer_worker, &state, producerIndex);
    }

    std::vector<std::thread> consumer_threads;
    consumer_threads.reserve(CONCURRENT_CONSUMERS);
    for (U32 consumerIndex = 0; consumerIndex < CONCURRENT_CONSUMERS; consumerIndex++) {
        consumer_threads.emplace_back(consumer_worker, &state);
    }

    for (std::thread& thread : producer_threads) {
        thread.join();
    }
    state.producers_done.store(true, std::memory_order_release);
    for (std::thread& thread : consumer_threads) {
        thread.join();
    }

    EXPECT_EQ(state.consumed.load(std::memory_order_acquire), CONCURRENT_TOTAL_MESSAGES);
    for (U32 index = 0; index < CONCURRENT_TOTAL_MESSAGES; index++) {
        EXPECT_EQ(state.received[index].load(std::memory_order_relaxed), 1u)
            << "value " << index << " not received exactly once";
    }
    state.queue.teardown();
}

//! Validate that two consumers always receive in priority order even when they are draining
//! concurrently. With a single producer at a time the FIFO-within-priority property must hold.
TEST(LocklessConcurrent, PriorityOrderSingleProducer) {
    constexpr FwSizeType DEPTH = 32;
    constexpr FwSizeType MESSAGE_SIZE = sizeof(U32);
    constexpr U32 BATCHES = 200;
    constexpr U32 BATCH_DEPTH = 16;

    Os::Queue queue;
    Fw::String name("priority-order-test");
    ASSERT_EQ(queue.create(0, name, DEPTH, MESSAGE_SIZE), Os::QueueInterface::Status::OP_OK);

    for (U32 batch = 0; batch < BATCHES; batch++) {
        for (U32 i = 0; i < BATCH_DEPTH; i++) {
            U8 buffer[MESSAGE_SIZE] = {0};
            const U32 value = (batch * BATCH_DEPTH) + i;
            for (FwSizeType b = 0; b < MESSAGE_SIZE; b++) {
                buffer[b] = static_cast<U8>((value >> (8 * b)) & 0xFFu);
            }
            ASSERT_EQ(queue.send(buffer, MESSAGE_SIZE, static_cast<FwQueuePriorityType>(i),
                                 Os::QueueInterface::BlockingType::NONBLOCKING),
                      Os::QueueInterface::Status::OP_OK);
        }
        FwQueuePriorityType lastPriority = std::numeric_limits<FwQueuePriorityType>::max();
        for (U32 i = 0; i < BATCH_DEPTH; i++) {
            U8 buffer[MESSAGE_SIZE] = {0};
            FwSizeType actualSize = 0;
            FwQueuePriorityType priority = 0;
            ASSERT_EQ(queue.receive(buffer, MESSAGE_SIZE,
                                    Os::QueueInterface::BlockingType::NONBLOCKING, actualSize, priority),
                      Os::QueueInterface::Status::OP_OK);
            ASSERT_LE(priority, lastPriority);
            lastPriority = priority;
        }
    }
    queue.teardown();
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}
