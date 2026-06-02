# Generic OSAL Implementations

This package provides generic implementations of various OSAL modules. These are implemented using generic data structures, other OSAL types, and C++ code. These specifically avoid calls down into any underlying operating system.

Available implementations:

1. [Os::PriorityQueue](#ospriorityqueue)
2. [Os::LocklessPriorityQueue](#oslocklesspriorityqueue)


## Os::PriorityQueue

Os::PriorityQueue is an in-memory implementation of Os::Queue. It allows projects that desire in-memory queue support to use Os::Queues.  Os::PriorityQueue allocates memory for its underlying data structures using `new (nothrow)` and deallocates memory with `delete`. These actions are taken during `create` and object destruction. This implies that Os::PriorityQueue should be instantiated and initialized during system initialization.

For memory protection, Os::PriorityQueue delegates to Os::Mutex and Os::ConditionVariable.

> [!WARNING]
> This Queue implementation is insufficient to be used for sending messages in ISR context due to the use of Os::Mutex as mentioned in above.

### Os::PriorityQueue Key Algorithms

Os::PriorityQueue stores messages in a set of dynamically allocated unordered parallel arrays. These arrays store: message data, and message data size respectively. There is also an index-free list that stores the indices that are available for storage in the fixed size arrays.

In order to prioritize messages, a [Types::MaxHeap](#typesmaxheap-data-structure) data structure is used.

When a message is received from a calling sender, `find_index` returns a free index from the free list. The data is copied into the message data array, and the size into the size array using this free index via `store_data`. The index is then inserted into the max heap structure for prioritization. When the queue is full and the `BLOCKING` option was supplied, the sender will block on the `m_full` condition variable until notified of a dequeue.

When a message is dequeued, the highest priority index is removed from the max heap. The data is copied out from the data array, and the size from the size array using that index via `load_data` and sent to the calling receiver. The index is then returned to the free list via `return_index` to indicate that it may be reused. When the queue is empty and the `BLOCKING` option was supplied, the receiver will block on the `m_empty` condition variable until notified of a enqueue.

If the queue is empty and data was received, the `m_empty` condition variable is notified to unblock waiting receivers. If the queue is full and data was dequeued, the `m_full` condition variable is notified to unblock waiting receivers.

### Types::MaxHeap Data Structure

The Types::MaxHeap data structure is used to prioritize a list of indices using the given priority. This heap uses a dynamically allocated maximum-length array to back a binary tree storage structure. The first element is the root of the tree, left children are calculated using `2x + 1` and right children using `2x + 2`. A node's parent is at `(x - 1)/2`.

When an index is pushed into this structure, it starts at the first unused element (first free child of some node) and iteratively swaps with its parent as long as the new nodes priority is larger than the parent. This ensures that the higher-priority elements are closer to the root of the tree. Specifically, the root *is* the highest priority element.

When an index is pulled from this structure, the root is removed as it is the highest priority data structure. The last leaf of the tree is elevated to the root and `heapify` is then called to restore the max-heap invariant of the data structure.

`heapify` starts at the newly ill-ordered root. It iteratively swaps this node with the highest-priority child until this node is the largest of the three (parent, left child, and right child) or until this node is swapped into a leaf position without children. The max-heap invariant is now restored.

## Os::LocklessPriorityQueue

Os::LocklessPriorityQueue is an ISR-safe, lockless implementation of Os::Queue that provides strict-priority delivery without requiring any operating-system lock. It is intended for flight-software contexts where a producer or consumer may run in interrupt context and therefore cannot block on an OS-level mutex or condition variable.

All memory is allocated exactly once during `create` through the registered `Fw::MemAllocator`. No allocation occurs during `send`, `receive`, `getMessagesAvailable`, or `getMessageHighWaterMark`. The non-blocking variants of `send` and `receive` use only lock-free atomic operations and bounded `memcpy`, making them safe to invoke from ISR context.

> [!NOTE]
> The blocking variants (`BlockingType::BLOCKING`) spin-wait and must not be invoked from ISR context.

### Os::LocklessPriorityQueue Key Algorithms

The queue stores messages in a fixed pool of pre-allocated slots. Each slot is governed by a four-state atomic state machine (`FREE -> WRITING -> READY -> READING -> FREE`) with an embedded ABA tag that prevents the ABA problem across concurrent producers and consumers.

Producers scan the slot array for a `FREE` slot, claim it via compare-exchange, populate the message data, and publish with a `release` store transitioning to `READY`. Consumers scan for the highest-priority `READY` slot (with FIFO tiebreak by sequence number), claim it via compare-exchange, copy the message out, and release back to `FREE`.

All non-blocking control paths are bounded by `depth * MAX_RETRY_PASSES`. The detailed algorithm and memory-ordering rationale are documented in [`docs/sdd.md` (top-level)](../../../docs/sdd.md).


