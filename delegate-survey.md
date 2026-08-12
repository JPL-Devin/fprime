# Survey: `Os::` delegate-pattern services (`lib/fprime/Os`)

Generated while extending `skills/osal-delegate-alias/SKILL.md` to `Os::Console`.
Purpose: identify which `Os::X` wrapper classes are "clean" (RawTime/Mutex-style —
every public member is declared on `XInterface`, so `Os::X` could alias a concrete
impl directly) vs. "mixed-mode" (Console-style — the wrapper adds singleton state
and/or extra base classes beyond what `XInterface` declares, so a direct alias
would break callers or silently drop behavior).

A service is **mixed-mode** if its `Os::X` wrapper class has ANY of:
- a `static X& getSingleton()` / `static void init()` pair (singleton lifecycle
  not expressible on the interface without adding process-global state to every
  impl), or
- inheritance from a class other than `XInterface` (e.g. `Fw::Logger`), or
- static convenience methods (`X::write(...)`, `X::getUsage(...)`, etc.) that
  wrap the singleton rather than delegating through the interface pointer alone.

## Clean — alias-compatible today (RawTime/Mutex pattern)

These wrapper classes (`Os::X final : public XInterface`) declare **no**
additional public surface beyond what `XInterface` already declares (plus
ctor/dtor/copy-semantics). Every method a caller invokes on `Os::X` exists on
`XInterface`, so a concrete impl (e.g. `Va416x0Os::SomeImpl`) can be aliased in
directly with no loss of caller-visible behavior — same situation as
`Os::RawTime` / `Os::Mutex` already handled by the skill.

- **`RawTime`** (`Os/RawTime.hpp`, `Os/RawTimeInterface.hpp`) — reference case,
  already alias-capable.
- **`Mutex`** (`Os/Mutex.hpp`, `Os/MutexInterface.hpp`) — already alias-capable
  (this session's target).
- **`ConditionVariable`** (`Os/Condition.hpp`) — `final : public
  ConditionVariableInterface`; no singleton, no extra base.
- **`Directory`** (`Os/Directory.hpp`) — `final : public DirectoryInterface`; no
  singleton, no extra base.
- **`File`** (`Os/File.hpp`) — `final : public FileInterface`; no singleton, no
  extra base (has a copy-constructor variant via `getDelegate(..., to_copy)`
  like `RawTime`, unproblematic).
- **`Queue`** (`Os/Queue.hpp`) — `final : public QueueInterface`; no singleton,
  no extra base.
- **`CountingSemaphore`** (`Os/CountingSemaphore.hpp`) — `final : public
  CountingSemaphoreInterface`; no singleton, no extra base.

## Mixed-mode — Console-style, NOT cleanly alias-compatible

These wrapper classes add a singleton (`init()`/`getSingleton()`) and/or extra
static convenience API and/or extra base classes not on `XInterface`. Directly
aliasing `Os::X` to a concrete impl would drop that behavior (callers of the
static API would fail to compile, or singleton-dependent side effects like
logger registration would silently stop happening).

- **`Console`** (`Os/Console.hpp`) — **worst case**. `class Console : public
  ConsoleInterface, public Fw::Logger` — inherits an entire unrelated framework
  class (`Fw::Logger`) that no concrete impl (`GdsConsole`, `SeggerConsole`,
  `PosixConsole`, `StubConsole`) derives from. Plus `init()`, `getSingleton()`,
  and static `write(...)` overloads. `getSingleton()` performs one-time
  `Fw::Logger::registerLogger(&s_singleton)` — critical side effect with no
  equivalent on any concrete impl. Widely called via `Fw::Logger::log(...)`
  (dozens of call sites in `Svc/`, `Drv/`, `Fw/`, `Os/Posix/Task.cpp`) rather
  than through `ConsoleInterface` directly.
- **`Task`** (`Os/Task.hpp`) — `final : public TaskInterface` but adds
  `init()`, `getSingleton()`, `getNumTasks()`, `registerTaskRegistry(...)`, and
  a nested `TaskRoutineWrapper` helper class used internally to bounce through
  `onStart()`. No `Fw::Logger`-style extra base, but the singleton +
  `TaskRegistry` registration is global state with no interface-level
  equivalent.
- **`FileSystem`** (`Os/FileSystem.hpp`) — `final : public FileSystemInterface`,
  private constructor (singleton-only construction), `init()`/`getSingleton()`,
  and a full parallel set of `static Status removeFile(...)` etc. convenience
  wrappers duplicating every instance method.
- **`Cpu`** (`Os/Cpu.hpp`) — `final : public CpuInterface` +
  `init()`/`getSingleton()` + static `getCount(...)`/`getTicks(...)` wrappers
  duplicating instance methods.
- **`Memory`** (`Os/Memory.hpp`) — `final : public MemoryInterface` +
  `init()`/`getSingleton()` + static `getUsage(...)` wrapper duplicating the
  instance method.

## Notes

- All 5 singleton-based services (`Console`, `Task`, `FileSystem`, `Cpu`,
  `Memory`) are driven through `Os::init()` (`Os/Os.cpp`), which calls each
  `X::init()` to force singleton construction at process startup — this pattern
  is intentional/by-design in fprime, not incidental.
- Among the mixed-mode set, `Console` is uniquely bad because of the
  `Fw::Logger` multiple-inheritance; `Task`/`FileSystem`/`Cpu`/`Memory` only add
  singleton lifecycle + redundant static wrappers (no foreign base class), so
  they'd be a smaller lift than `Console` if ever pursued (still requires
  deciding how `init()`/`getSingleton()` map onto a directly-aliased concrete
  type, since none of the concrete impls implement those statics either).
- This survey only covers `lib/fprime/Os/*.hpp` top-level wrapper classes, not
  `Os/Generic/`, `Os/Models/`, or platform-specific (`Os/Posix`, `Os/Stub`,
  etc.) implementation headers.
