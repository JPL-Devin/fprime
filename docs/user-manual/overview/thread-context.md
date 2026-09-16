# Thread-Context Matrix: "Where Does My Code Run?"

One of the most common sources of confusion for new F' developers is understanding **which thread** executes a given handler. This page provides a quick-reference matrix that makes execution context explicit.

## Thread-Context Matrix

The thread that runs your handler depends on three factors: the **handler type**, the **component kind**, and the **port kind**.

| Handler Type | Component Kind | Port Kind | Execution Thread |
|---|---|---|---|
| Command handler | Passive | `sync` | Command Dispatcher's thread |
| Command handler | Passive | `guarded` | Command Dispatcher's thread (mutex-protected) |
| Command handler | Active | `async` | Component's own thread (via queue) |
| Command handler | Active | `sync` | Command Dispatcher's thread |
| Command handler | Active | `guarded` | Command Dispatcher's thread (mutex-protected) |
| Input port handler | Passive | `sync` | Caller's thread |
| Input port handler | Passive | `guarded` | Caller's thread (mutex-protected) |
| Input port handler | Active | `async` | Component's own thread (via queue) |
| Input port handler | Active | `sync` | Caller's thread |
| Input port handler | Active | `guarded` | Caller's thread (mutex-protected) |
| Input port handler | Queued | `async` | Thread that triggers dispatch |
| Input port handler | Queued | `sync` | Caller's thread |
| Input port handler | Queued | `guarded` | Caller's thread (mutex-protected) |
| Rate group `schedIn` handler | Passive | `sync` | Rate group's thread |
| Rate group `schedIn` handler | Active | `async` | Component's own thread (via queue) |

> [!NOTE]
> Even on an **active component**, `sync` and `guarded` port handlers execute on the **caller's thread**, not the component's own thread. Only `async` handlers use the component's queue and thread.

## How to Read This Table

1. **Find your handler type** -- Is it a command handler, a port handler, or a rate group schedule handler?
2. **Identify your component kind** -- Is your component `passive`, `active`, or `queued`?
3. **Check the port kind** -- Is the port declared as `sync`, `async`, or `guarded`?
4. **Read the execution thread** -- This tells you which thread will run your code.

## Key Implications

### Thread Safety

If your handler runs on the **caller's thread** (any `sync` or `guarded` handler), you share that thread with the caller. This means:

- Your handler **must not block** or perform long-running operations -- doing so delays the caller.
- If multiple callers can invoke the same `sync` handler concurrently, you must protect shared state manually.
- `guarded` handlers automatically serialize access using a component-wide mutex, providing thread safety at the cost of potential contention.

### Async Handlers and Queues

If your handler runs on the **component's own thread** (`async` handlers on active components):

- The invocation is decoupled from the caller -- the caller returns immediately after enqueuing.
- Messages are dispatched in FIFO order, so handlers on the same component do not run concurrently with each other.
- Queue depth must be sized appropriately. An undersized queue can lead to assertion failures.

### Rate Group Context

Components attached to a rate group via `schedIn`:

- **Passive components** with a `sync` `schedIn` port execute directly on the rate group's thread. This is synchronous -- the rate group waits for the handler to return before calling the next component.
- **Active components** with an `async` `schedIn` port receive a message on their queue. The rate group does not wait -- it proceeds to the next component immediately.

> [!WARNING]
> A passive component performing slow work in its `schedIn` handler will delay all subsequent components in the same rate group cycle, potentially causing a rate group slip.

## Common Patterns

### Pattern 1: Passive Sensor Reader in a Rate Group

```
Rate Group Thread --> sensor.schedIn (sync) --> reads hardware --> returns
```

The sensor read happens on the rate group's thread. Keep it fast.

### Pattern 2: Active Controller Receiving Commands

```
Dispatcher Thread --> controller.cmdHandler (async) --> enqueues message --> returns
Controller Thread --> dequeues --> executes command --> sends response
```

The command handler runs on the controller's own thread, decoupled from the dispatcher.

### Pattern 3: Guarded Data Access

```
Thread A --> dataStore.getData (guarded) --> mutex lock --> read data --> mutex unlock --> returns
Thread B --> dataStore.getData (guarded) --> waits for mutex --> read data --> mutex unlock --> returns
```

Both callers execute the handler on their own threads, but the mutex ensures only one runs at a time.

## Further Reading

- [Execution Model Primer](execution-model.md) -- high-level overview of how F' executes
- [Core Constructs: Ports, Components, and Topologies](03-port-comp-top.md) -- port kinds and component types in detail
- [Rate Groups and Timeliness](../design-patterns/rate-group.md) -- rate group scheduling
