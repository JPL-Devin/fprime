# CmdSequencer Directives — Review Actions

Reviews of the sequence-directive feature (`executeDirective`, `jumpToLabel`,
`performCmd_Step`, `cmdResponseIn_handler`, and the new member variables).

| Review | Source | Findings | Status |
| --- | --- | --- | --- |
| Part A — C/C++ design | `.github/skills/fprime-cpp-design/SKILL.md` (CPP-1 … CPP-37) | 3 must-fix, 2 could-fix, 3 suggestions | fixed |
| Part B — Correctness | `.github/agents/correctness-review.agent.md` | 5 must-fix, 2 could-fix | fixed |
| Part C — Security | `.github/agents/security-review.agent.md` | 1 must-fix, 3 suggestions, 1 future work | fixed (C3 not taken, by decision) |
| Part D — Found while implementing A–C | — | 1 must-fix | fixed |
| Part E — Second round, on the A–D fixes | C/C++ design + test quality | 3 must-fix, 7 should-fix, 5 nit | **open — none implemented** |

All 31 Parts A–D findings are implemented, documented in `docs/sdd.md`, and
covered by a new 30-test directive suite; all 109 unit tests pass. Part E is
recorded only.

**CI safety: No-Go. Merge readiness: No-Go.** Part E's second review round is
incomplete — the correctness and security reviewers did not run, and security
is a CI-safety contributor. Part E also has 3 outstanding must-fix findings.
See the gap notice in Part E.

---

## Picking this up later

### State of the work

Nothing is committed. All of Parts A–D plus the new test suite live in the
**uncommitted working tree** on branch `seq-directives`:

- Modified: `CmdSequencer.fpp`, `Events.fppi`, `CmdSequencerImpl.{hpp,cpp}`,
  `CMakeLists.txt`, `docs/sdd.md`, `actions.md`,
  `test/int/test_cmd_sequencer.py`, `test/ut/CmdSequencerMain.cpp`,
  `test/ut/SequenceFiles/File.cpp`,
  `test/ut/SequenceFiles/AMPCS/CRCs.cpp`,
  `test/ut/SequenceFiles/FPrime/Records.{hpp,cpp}`
- New (untracked): `test/ut/Directives.{hpp,cpp}`,
  `test/ut/SequenceFiles/DirectiveFile.{hpp,cpp}`

`git diff` and `git status` in `Svc/CmdSequencer` are the authoritative view.
Nothing outside `Svc/CmdSequencer` was touched.

### Build and test

```sh
cd <repo root> && source fprime-venv/bin/activate
cd Svc/CmdSequencer
fprime-util build --ut -j$(nproc)     # NOT `generate --ut` — see below
fprime-util check -j$(nproc)          # NO positional argument — see below
```

Two traps that cost time before:

- **`fprime-util generate --ut` fails** with `InvalidBuildCacheException` once
  the cache exists. Use `build --ut`, which regenerates as needed.
- **`fprime-util check -j$(nproc) Svc/CmdSequencer` from the repo root
  fails** — the positional argument is parsed as a build-cache suffix. You
  must `cd` into the module directory and pass no positional argument.

Expect `[ PASSED ] 109 tests` (79 pre-existing + 30 new `Directives.*`). The
UT target re-adds `-Wno-conversion` (`CMakeLists.txt:74`) but keeps
`-Werror -Wshadow -Wold-style-cast`, and runs under ASAN/UBSAN/LSAN.

### Constraints the test suite depends on

- **Sequence file base names must be ≤ 23 characters.**
  `Fw::CmdStringArg` is capped at `FW_CMD_STRING_MAX_SIZE` = 40
  (`default/config/FpConstants.fpp:29`), and `File::setName` builds
  `bin/f_prime_<base>.bin`. A longer name is silently truncated by the
  `CS_RUN` argument, and the component then reports `CS_FileNotFound` — which
  looks like a missing-file bug, not a too-long-name bug. Four tests hit this.
  `DirectiveFile`'s constructor now guards it with an explicit `EXPECT_LT`, so
  the failure names itself. Keep the guard.
- **Every directive test depends on zero time.**
  `prepare()` sets the test time to `Fw::Time(TB_WORKSTATION_TIME, 0, 0)` and
  `DirectiveFile::command()` writes relative offset 0, which together make
  command records fire immediately so no test has to drive `schedIn`. Break
  either side and most of the 30 tests fail on
  `ASSERT_from_comCmdOut_SIZE` with no hint of the shared cause. This is E-T8.
- **`runSequence()` asserts `ASSERT_EVENTS_SIZE(1)`,** so it cannot be used by
  a test whose first step logs more than the load event. Those tests use the
  local `sendRun()` helper instead. This is why both exist.
- **`DirectiveFile` sequence shapes are load-bearing.** The assertions were
  derived by tracing the implementation's exact event ordering; changing a
  shape breaks assertions in non-obvious ways. `DirectiveCycleDetected` is the
  most delicate — reaching `ERROR_DIRECTIVE_CYCLE` requires a jump target at
  least two records *before* the jump, so the step reads more records than the
  sequence holds. It pins its own record count with
  `ASSERT_EQ(4U, file.getNumRecords())`.
- **Never hand-edit `*Ac.hpp/cpp`.** Change the `.fpp` / `.fppi` model and
  rebuild. Several Part E fixes (E-D1, E-D2) require model changes, so they
  will regenerate autocode and shift generated assertion signatures.

### What to do next, in order

1. **Re-run the correctness and security reviewers** — they did not run in the
   Part E round and their gap is the reason both verdicts are No-Go. Do this
   *before* implementing Part E, so their findings merge into one fix pass
   rather than forcing a third round. Scope: the working-tree changes to
   `Svc/CmdSequencer` plus the four new test files. See the Part E preamble
   for how they were invoked without a PR.
2. **Fix Part E's 3 must-fix items** (E-D1, E-D2, E-T1) plus whatever the two
   missing reviewers surface. E-D1 and E-D2 both change the FPP model, so do
   them together and rebuild once. E-D2 also changes the expected value at
   `test/ut/Directives.cpp:389` and in E-T5's suggested test.
3. **Then the should-fix items.** E-D7 and E-T4 are one defect — fix once.
   E-D3 and E-D4 interact: the `StepResult` enum from E-D4 makes E-D3's
   correct behavior explicit rather than inferred, so E-D4 first is cheaper.
4. **Mirror every implementation change in `docs/sdd.md`** and extend the unit
   tests, as with Parts A–D. `sdd.md`'s change log has a 9/26/2026 row for the
   Parts A–D work; add a row rather than editing that one.
5. **Resolve E-T2's open question** — it needs a maintainer decision, not just
   an edit. See the finding for the three options.

### Decisions already made — do not relitigate

- **JCF strict adjacency is intended.** A JCF must be the record *immediately*
  after the failed command; an intervening `LABEL` or `ERROR_MODE` kills the
  sequence under error mode ON. This was raised as a design question, decided
  in favor of keeping the behavior, and `docs/sdd.md` was corrected to match
  (it had read as though intervening directives were tolerated). See
  "Routed elsewhere" at the end of this file for the original framing.
- **A5 was not implemented as written, deliberately.**
  `Fw::StringBase::deserializeFrom` reads an `FwSizeStoreType`
  length prefix, but directive labels are a bare `U8` length plus raw chars.
  Using it would silently change the wire format. The rationale is recorded as
  a comment in `deserializeLabel` and in `sdd.md`.
- **C3 is dispositioned "not taken"** with a rationale in the finding itself.
  The security reviewer should confirm or overturn that call when it re-runs.
- **The dead `m_jcfActive` / `m_jcfTarget` / `m_jcsActive` / `m_jcsTarget`
  state was deleted**, not kept for compatibility.

---

## Part A — C/C++ Design (CPP rules)

---

### A1. Extract the duplicated label-parse block

- [x] Add a private helper `deserializeLabel(Fw::ExternalSerializeBuffer&, Fw::String&)`
- [x] Replace the three inlined copies with calls to it

**Rule:** CPP-33 `cpp-inlined-utility` — **could fix**

**Sites:** `CmdSequencerImpl.cpp:666-690` (JCF), `:747-771` (JCS), `:848-861` (`jumpToLabel`)

The block — deserialize length, bounds-check against 20, deserialize chars
with `OMIT_LENGTH`, null-terminate — is triplicated verbatim. It depends on
no component state and operates on general types.

---

### A2. Justify or remove the casts

- [x] `const_cast<U8*>(...getBuffAddr())` at `CmdSequencerImpl.cpp:621`
- [x] `const_cast<U8*>(...getBuffAddr())` at `CmdSequencerImpl.cpp:836`
- [x] `reinterpret_cast<U8*>(labelBuf)` at `:684`, `:765`, `:856` — these disappear if A5 is done

**Rule:** CPP-10 `cpp-reinterpret-or-const-cast-unjustified` — **must fix**

Five casts on a flight path with no inline justification. SKILL §3 upgrades
unjustified `const_cast` / `reinterpret_cast` on flight code to must-fix.

The `const_cast` pair is the more serious shape: it strips const off a record
buffer to hand to `ExternalSerializeBuffer`. Either use a const-correct
deserializer entry point, or add a comment stating the buffer is not mutated
and why the API forces non-const.

---

### A3. Make the new event emissions traceable

- [x] Add a distinct event for directive jumps, carrying the target label
- [x] Stop emitting `CS_SequenceCanceled` at `:699` and `:779`
- [x] Give `CS_InvalidMode` an FPP enum argument identifying the call site

**Rule:** CPP-36 `cpp-event-not-uniquely-traceable` — **must fix**

Two problems:

`CS_SequenceCanceled(fileName)` now fires from 4 sites (`:221`, `:232`,
**`:699`**, **`:779`**) with an indistinguishable argument set. Worse, at
699/779 the event is factually wrong — the sequence *jumped*, it wasn't
canceled. An operator cannot tell a JCF jump from a real cancel.

`CS_InvalidMode()` takes **no arguments** and now fires from 6 sites (`:96`,
`:163`, `:438`, `:488`, **`:661`**, **`:742`**). At 661/742 it means "JCF/JCS
before any command ran" — unrecoverable from the log alone.

Sites 221/232/96/163/438/488 are preexisting → **future work**. The four new
emissions are in scope.

---

### A4. Initialize the new locals

- [x] `directiveId` at `:625` and `:840`
- [x] `labelLen` at `:667`, `:748`, `:848`
- [x] `labelBuf` at `:676`, `:757`, `:854`
- [x] `exitStatus` at `:717`
- [x] `mode` at `:797`

**Rule:** CPP-19 `cpp-uninitialized-variable` — **must fix**

Each is written by `deserializeTo` before use, so there is no UB today — but
CPP-19 is unconditional and SKILL §3 forbids downgrading outside test code.
One-line fix each:

```cpp
U8 directiveId{};
```

Most of these vanish with A1.

---

### A5. Use `Fw::String` for the label buffer

- [x] Replace `char labelBuf[21]` at `:676`, `:757`, `:854`

**Rule:** CPP-24 `cpp-char-pointer-where-fw-string-fits` — **suggestion**

The buffer plus manual null-termination is converted to `Fw::String` at `:695`
and `:776` anyway. Deserialize into an `Fw::String` (or an `Fw::ExternalString`
over the fixed buffer) and drop the hand-rolled terminator. This also removes
the three `reinterpret_cast` sites in A2.

---

### A6. Name the label-size constants

- [x] Add constants to the `Record::Constants` enum
- [x] Replace literals at `:676-678`, `:757-759`, `:850-854`

**Rule:** CPP-30 `cpp-magic-number-replacing-constant` — **could fix**

The 20-char label limit and its 21-byte buffer are bare literals at six
points, with the `+1` relationship implicit. The `Record::Constants` enum next
door is the idiomatic home:

```cpp
enum Constants { MAX_LABEL_SIZE = 20, LABEL_BUFFER_SIZE = MAX_LABEL_SIZE + 1 };
```

---

### A7. Return a status enum from `executeDirective`

- [x] Replace the `bool` return at `CmdSequencerImpl.hpp:671`

**Rule:** CPP-35 `cpp-bool-status-where-enum-fits` — **suggestion**

The `bool` collapses six distinct outcomes: malformed record, unknown directive
ID, directive before any command, label not found, bad exit status, bad error
mode. The caller at `CmdSequencerImpl.cpp:529` can only `performCmd_Cancel()`.
An FPP `DirectiveStatus` enum would let the cause reach the ground.

Not upgraded to must-fix because each cause currently logs its own event
before returning.

---

### A8. Bound the label search loop

- [x] Convert the `while` at `CmdSequencerImpl.cpp:830` to a bounded `for`

**Rule:** CPP-34 `cpp-while-loop-for-counted-iteration` — **suggestion**

`while (this->m_sequence->hasMoreRecords())` walks the record list with no
visible upper bound; `nextRecord` returns `void`, so termination rests
entirely on `hasMoreRecords` being monotonic. CPP-27 / JPL requires a provable
bound. `getHeader().m_numRecords` is available — a `for` over it makes the
bound explicit and the three `continue` paths safe by construction.

---

## Part B — Correctness

All seven are confirmed against the source: each has a concrete triggering
record sequence, a traced path to the wrong outcome, and no upstream guard
that prevents it. Four (B1, B2, B3, B4) were introduced by the
JCF/JCS-after-command redesign; B5 is inherent to the redesign's state
model; B6 and B7 are pre-existing shapes that the redesign made reachable.

`m_errorPendingAbort` is the thread running through B2, B3, B4 and B7 — a
single flag cleared in three places and not in the fourth. Consider fixing
them as one change.

---

### B1. Directive jump loop recurses without bound

- [x] Clear `m_lastCmdExecuted` / `m_lastCmdStatus` after a JCF or JCS jump
- [x] Convert the directive continuation at `CmdSequencerImpl.cpp:535` from recursion to iteration

**Class:** `correctness-nontermination` — **must fix**

`performCmd_Step` (`:532-535`) calls **itself** to advance past a directive.
`m_lastCmdStatus` is never cleared once a JCF/JCS has consumed it, so a
backward jump that reaches its own JCF again with no command in between
recurses forever.

Trigger — three records, no malformed input required:

```text
RELATIVE  <command that returns anything but OK>
LABEL     "RETRY"
JCF       "RETRY"
```

Trace: command fails → `cmdResponseIn_handler:340` stores
`m_lastCmdStatus = <failure>` → `performCmd_Step` → `LABEL` is a no-op →
`performCmd_Step` → `JCF` sees `m_lastCmdStatus != OK` → `jumpToLabel("RETRY")`
rewinds the deserializer to just past `LABEL "RETRY"` → returns `true` →
`:535` recurses → reads `JCF "RETRY"` again → `m_lastCmdStatus` is *still* the
failure → jumps again. No frame ever returns.

Consequence: stack exhaustion, then FSW abort. Occurs under both error modes
(with ERROR_MODE ON the first JCF clears `m_errorPendingAbort`, so the guard
at `:644` does not stop the second pass).

The recursion is also unbounded in the benign case: a run of *N* consecutive
directives costs *N* stack frames, so a long `LABEL` block is a stack-depth
hazard independent of B1's loop. Both go away if the continuation becomes a
loop over records rather than a self-call.

---

### B2. `m_errorPendingAbort` survives into the next sequence

- [x] Add `this->m_errorPendingAbort = false;` to `sequenceComplete` (`CmdSequencerImpl.cpp:551-579`)

**Class:** `correctness-state-machine` — **must fix**

`sequenceComplete` clears `m_executedCount`, `m_lastCmdExecuted`,
`m_lastCmdStatus` and `m_errorMode` (`:557-565`) — but not
`m_errorPendingAbort`. Only `performCmd_Cancel` (`:312`) and the constructor
clear it.

Trigger: a sequence whose **last record is a failing command** — legal,
because `validateRecords` (`FPrimeSequence.cpp:303`) does not require a
terminating `END_OF_SEQUENCE` record, and `CS_STEP_cmdHandler:443` documents
sequences without one as a supported case.

Trace: command fails with ERROR_MODE ON → `m_errorPendingAbort = true`
(`:349`) → `cmdResponseIn_handler:355` finds `not hasMoreRecords()` →
`sequenceComplete()` → flag stays `true` with no sequence running.

Consequence: the **next** sequence run is killed at its first non-directive
record by the guard at `:508`, emitting `CS_SequenceCanceled` with no event
explaining the cause. One poisoned sequence per occurrence; `CS_RUN` of a
valid file appears to fail for no reason.

```cpp
// in sequenceComplete, alongside the existing resets
this->m_errorMode = true;
this->m_errorPendingAbort = false;
```

---

### B3. A failed final command reports sequence success

- [x] In `cmdResponseIn_handler`, take the error exit when a failure leaves no more records

**Class:** `correctness-state-machine` — **must fix** (regression)

Same trigger as B2 — last record is a failing command, no `END_OF_SEQUENCE`.
Both the AUTO branch (`:355-358`) and the MANUAL branch (`:364-367`) call
bare `sequenceComplete()`, which defaults to `Fw::CmdResponse::OK`. So
`seqDone_out` and `cmdResponse_out` both report **OK**, and
`CS_SequenceComplete` is logged, for a sequence whose last command failed
with ERROR_MODE ON.

Before the redesign this path called `performCmd_Cancel()` and reported
`EXECUTION_ERROR`. Consequence: ground and any upstream sequencer see a
clean completion; a blocking `CS_RUN` returns success. Failure is invisible
except for the separate `CS_CommandError` event.

```cpp
if (not this->m_sequence->hasMoreRecords()) {
    this->m_runMode = STOPPED;
    if (this->m_errorPendingAbort) {
        this->performCmd_Cancel();      // reports EXECUTION_ERROR, clears the flag
    } else {
        this->sequenceComplete();       // ERROR_MODE OFF: failure was accepted
    }
}
```

---

### B4. Non-JCF directive after a failed command cancels twice

- [x] Make the guard at `CmdSequencerImpl.cpp:643-649` signal the caller instead of canceling itself

**Class:** `correctness-state-machine` — **must fix**

`executeDirective`'s pending-abort guard calls `performCmd_Cancel()` *and*
returns `false` (`:646-648`). Its only caller, `performCmd_Step:529-531`,
treats `false` as "abort" and calls `performCmd_Cancel()` again.

Trigger: ERROR_MODE ON (the default), a failing command, and any non-`JCF`
directive next — `LABEL`, `JCS`, `EXIT`, or `ERROR_MODE`:

```text
RELATIVE  <command that fails>
LABEL     "CLEANUP"
```

Consequence: `seqDone_out(0, 0, 0, EXECUTION_ERROR)` is invoked **twice** for
one sequence run (`:315` has only an `isConnected` guard). A downstream
sequencer or counter sees two completions for one sequence. `cmdResponse_out`
is spared only incidentally, because the first call leaves `m_blockState ==
NO_BLOCK` and `m_join_waiting == false`.

Fix: drop the `performCmd_Cancel()` from the guard and let the caller own the
cancel — or, with A7, return a dedicated status the caller dispatches on.

---

### B5. JCF/JCS reached by a jump tests a stale command status

- [x] Clear `m_lastCmdExecuted` / `m_lastCmdStatus` once a JCF/JCS jump is taken

**Class:** `correctness-state-machine` — **must fix**

`m_lastCmdStatus` is written only by `cmdResponseIn_handler:340` and cleared
only in `performCmd_Cancel` / `sequenceComplete`. It is therefore sticky
across jumps: a JCF/JCS reached by control flow rather than by falling out of
a command still evaluates the status of whatever command last ran.

Trigger: an error handler entered by a jump that itself begins with a
conditional directive:

```text
RELATIVE  <command A, fails>
JCF       "HANDLER"
...
LABEL     "HANDLER"
JCS       "DONE"          <-- tests command A, several records back
```

Consequence: the wrong branch is taken. A handler cannot distinguish "no
command has run since I was entered" from "the command before my caller
succeeded", so recovery logic silently takes the success path. This is also
what makes B1's loop unbounded, so the same fix addresses both.

`docs/sdd.md:309` deliberately allows *consecutive* JCF/JCS after one command
to all test that command — so clear on **jump taken**, not on every
evaluation, to preserve the documented behavior.

---

### B6. `m_executedCount` freezes on a failed command

- [x] Advance `m_executedCount` on the failure path in `cmdResponseIn_handler:345`

**Class:** `correctness-other` — **could fix**

`commandComplete` (`:581-587`) increments `m_executedCount` and is called
only on the success path. The failure path calls
`commandError(this->m_executedCount, ...)` and increments nothing.

Trigger: ERROR_MODE OFF with commands A (fails) and B (succeeds). A's
`CS_CommandError` reports index 0; B's `CS_CommandComplete` also reports
index 0.

Consequence: after the first failure, every subsequent `CS_CommandComplete`,
`CS_CommandError` and `CS_SequenceTimeout` reports a record index low by the
number of prior failures, and `CS_RecordInvalid` in `executeDirective`
inherits the same skew. The operator cannot map events back to records.
Diagnostics only — no control-flow effect, hence could-fix. Before the
redesign a failure ended the sequence, so the counter never had to survive
one.

---

### B7. `CS_STEP` reports OK after aborting the sequence

- [x] Check `m_runMode` before the response at `CmdSequencerImpl.cpp:454`

**Class:** `correctness-other` — **could fix**

Trigger: MANUAL step mode, ERROR_MODE ON, a command fails →
`m_errorPendingAbort = true` and the handler waits (`:364`). The operator
sends `CS_STEP` → `performCmd_Step` hits the guard at `:508` →
`performCmd_Cancel()` → `m_runMode = STOPPED`.

Back in `CS_STEP_cmdHandler`, `:451` suppresses `CS_CmdStepped` because
`m_runMode == STOPPED`, but `:454` still sends
`cmdResponse_out(..., Fw::CmdResponse::OK)`.

Consequence: `CS_STEP` acknowledges success while it in fact terminated the
sequence. The pre-existing shape is unchanged, but the guard at `:508` gave
it a new way to be reached; before the redesign a failing command in MANUAL
mode had already canceled the sequence, so no `CS_STEP` followed.

---

## Part C — Security

**Source classification.** `Svc::CmdSequencer` is named in
`fprime-ground-input-tracing` §1 twice over — as a detector component whose
scanned payload is ground-input, and as a consumer of uplinked file content.
So **every byte of a sequence record, including all directive payloads, is
`ground-input` at zero hops.** High confidence; no topology trace needed.

**What the directive code gets right.** Categories 1, 3, 4 and 6 produced no
findings, which is worth stating rather than leaving implicit:

- Every `deserializeTo` in `executeDirective` and `jumpToLabel` has its status
  checked before the value is used.
- Every ground-supplied scalar is range-checked before use: `directiveId <=
  ERROR_MODE` (`:634`), `labelLen <= 20` (`:678`, `:759`, `:850`), `exitStatus
  <= 1` (`:726`), `mode <= 1` (`:806`).
- No `memcpy`, no allocation, and no queue push takes a ground-controlled
  size. The only ground-sized copy is the label read, bounded above.

The one memory-write on a ground-controlled index, `labelBuf[readSize] =
'\0'`, is in-bounds: `readSize <= 20` is enforced before the call, and
`deserializeTo` only ever lowers it. `char labelBuf[21]` — max index 20.

---

### C1. Ground-controlled recursion depth — uplink-triggerable stack exhaustion

- [x] Convert the directive continuation at `CmdSequencerImpl.cpp:535` to iteration

**Class:** `general-vulnerability/unbounded-recursion` — **must fix**

*Concurrence with **B1** — same site-key, same fix. Recorded here for the
ground-reachability classification and the DoS consequence, not as a second
defect.*

`performCmd_Step` recurses into itself once per consecutive directive
(`:532-535`). The recursion depth is set by the number of consecutive
`SEQUENCE_DIRECTIVE` records in the uplinked file — a `ground-input` count
with **no bound check**, which is category 2's shape applied to stack rather
than heap.

Quantified for the Ref deployment, which allocates a 5 KB sequence buffer
(`TestDeploymentsProject/Ref/Top/RefTopology.cpp:60`): a minimal `LABEL`
record is 15 bytes — descriptor 1 + time tag 8 + record size 4 + payload 2 —
so a single valid 5 KB sequence file yields **~341 records**, hence ~341
nested `performCmd_Step` → `executeDirective` frame pairs. Each pair carries
an `ExternalSerializeBuffer`, a `char[21]`, and an `Fw::Time`.

Three distinct ways ground reaches this, in increasing severity:

1. **A well-formed sequence with a long directive run** — no malformed input,
   no loop. Depth scales with the sequence buffer size.
2. **A well-formed retry loop** — because each taken jump also consumes a
   frame that is never popped, a legitimate `LABEL`/`JCF` retry loop grows the
   stack linearly in *iterations executed*, not in file size. A sequence
   author writing an ordinary bounded retry crashes the FSW.
3. **Unbounded** — the `LABEL "R"` / `JCF "R"` construction in B1 recurses
   forever.

Consequence: stack overflow in the sequencer task, then FSW abort. I could not
resolve the sequencer task's stack budget (`Default::STACK_SIZE` is not
defined within read scope), so the exact file size that overflows is
deployment-specific — but cases 2 and 3 exceed any finite budget.
*Low confidence on the numeric threshold only; the unboundedness is certain.*
Per `maintainer-lookup`, security-overseer `@bitWarrior` should confirm the
per-deployment stack budget.

Converting `:535` to a loop fixes all three, and is the same edit B1 needs.

---

### C2. `jumpToLabel` discards malformed uplinked directives silently

- [x] Emit `CS_RecordInvalid` at the three `continue` sites (`:843`, `:851`, `:859`)

**Class:** `ground-validation-gap` — **suggestion**

`jumpToLabel` skips any directive it cannot parse — bad directive ID (`:843`),
bad or oversized label length (`:851`), failed label read (`:859`) — with a
bare `continue` and **no event**. Every other rejection of malformed ground
input in this component logs `CS_RecordInvalid`.

Consequence: a malformed `LABEL` record is invisible to the ground. The jump
either silently resolves to a *different* label further on or silently fails
the whole sequence with only a generic `CS_CommandError`, and the operator has
no evidence which record was rejected or why. Malformed uplinked content
should never be discarded without a trace.

Related, same function: `jumpToLabel` returns the **first** matching label and
nothing at load time rejects duplicate `LABEL` names, so a sequence with two
`LABEL "X"` records always jumps to the first. Worth a validation at load.

```cpp
if (status != Fw::FW_SERIALIZE_OK || labelLen > 20) {
    this->log_WARNING_HI_CS_RecordInvalid(this->m_sequence->getLogFileName(),
                                          this->m_executedCount, labelLen);
    continue;
}
```

---

### C3. Label search rescans from record 0 on every jump

- [x] Disposition: **not taken.** Rationale below.

**Class:** `general-vulnerability/unbounded-loop` — **suggestion**

`jumpToLabel:827` calls `reset()` and rescans the whole record list on every
jump, synchronously on the sequencer's thread. Work is O(records) per jump and
O(records²) over a sequence that jumps often — with the record count
ground-controlled.

The loop does terminate: `validateRecords` (`FPrimeSequence.cpp:303-331`)
proves every record deserializes and consumes the buffer exactly, so
`hasMoreRecords()` is monotonic. At Ref's 5 KB buffer the worst case is ~341²
≈ 116 K record deserializations — milliseconds, not a denial of service on its
own, which is why this is a suggestion and not a must-fix. It is listed
because it is a ground-scaled cost on a blocking path: `CS_RUN` in `BLOCK`
mode holds the caller's command response for the duration.

**Disposition — not taken.** A label index is not free: it needs a fixed-size
table sized at configuration time (no dynamic allocation is permitted), which
adds a new configuration constant, a new overflow failure mode when a sequence
declares more labels than the table holds, and a second place where the label
set can disagree with the file. That is a worse trade than milliseconds of
scan. Two changes made for A8 and B1 bound the cost instead:

- `jumpToLabel` is now a counted `for` over `getHeader().m_numRecords`
  (A8), so a single search is bounded by the validated record count rather
  than by `hasMoreRecords()`.
- `performCmd_Step` bounds the number of records a single step may read to
  `m_numRecords` and reports `ERROR_DIRECTIVE_CYCLE` past that (B1/C1), so
  the number of jumps per step is bounded too. The O(records²) figure is
  therefore a per-step ceiling, not an unbounded accumulation.

Revisit only if a project runs sequences large enough for the scan to matter
against its `CS_RUN` response deadline.

---

### C4. Integration test uplinks to a fixed path outside the working tree

- [x] Replace `/tmp/ref_test_seq*.bin` with a unique per-run destination
      (`Svc/CmdSequencer/test/int/test_cmd_sequencer.py:169-172` and its 7 reuses)

**Class:** `ci-test-runtime-policy-violation` — **suggestion**

The new integration test uplinks to hard-coded `/tmp/ref_test_seq.bin` and
`/tmp/ref_test_seq_wait.bin`. Per `ci-test-runtime-policy`, writing outside
the working tree at a predictable path is a category-8 trigger: on a shared
runner the destination is world-writable and pre-creatable, and two concurrent
CI runs collide on the same filename.

Does **not** force `CI safety: No-Go` — it is a `**suggestion**`, not a
`**must fix**`, and the rule gates on outstanding category-8 must-fixes only.

Clean in the same file, worth noting so it is not "fixed" into something
worse: the `fprime-seqgen` invocations at `:140-165` use `subprocess.run` with
a **list** argument and no `shell=True`. That is the correct form; keep it.

---

### C5. Shell-interpreted `system()` in the unit-test path

- [x] Replace with `Os::FileSystem::removeFile` in
      `test/ut/SequenceFiles/File.cpp:95` and
      `test/ut/SequenceFiles/AMPCS/CRCs.cpp`

**Class:** `ci-test-runtime-policy-violation` — **future work**

`File::remove()` builds `"rm -f " + name` and calls `system()`, spawning a
shell during every unit-test run — a category-8 primitive.

Tagged `**future work**`, not `**must fix**`: the branch only reformatted
these lines (`git diff` shows the `system()` call present on both sides), so
the behavior is neither introduced nor widened, and the concatenated path is a
compile-time constant, so no untrusted value reaches the shell. Replacing it
removes the shell from the test path entirely.

---

## Part D — Found while implementing Parts A–C

These were not in the original three reviews. They were found by reading the
code closely enough to change it, and are fixed in the same pass.

### D1. `CS_RUN` in `BLOCK` mode sends two command responses when the sequence
ends inside the first step

- [x] Capture the requested block state before stepping
      (`CmdSequencerImpl.cpp`, `CS_RUN_cmdHandler`)

**Class:** `correctness/double-response` — **must fix**

`CS_RUN_cmdHandler` decided whether it owed a command response by re-reading
`this->m_blockState` *after* calling `performCmd_Step`. But a sequence can end
inside that step — an `EXIT` directive as the first record, an immediately
empty record list, or an abort — and `sequenceComplete`/`performCmd_Cancel`
both send the owed response and then clear `m_blockState` to `NO_BLOCK`. The
handler then saw `NO_BLOCK`, concluded it owed a response, and sent a **second**
one for the same opcode/`cmdSeq`.

A duplicate response to `Svc::CommandDispatcher` for an opcode it is no longer
tracking is a dispatcher-level error, not a sequencer one, so the symptom
surfaces far from the cause.

Fixed by reading the block state the command actually asked for, once, before
any stepping:

```cpp
const Svc::BlockState::t requestedBlock = block.e;
...
if (Svc::BlockState::NO_BLOCK == requestedBlock) {
    this->cmdResponse_out(opCode, cmdSeq, ...);
}
```

This defect predates the directive work for the empty-sequence case, but the
`EXIT` directive is what makes it reachable with a non-trivial sequence, so it
belongs to this change.

---

## Part E — Second review round, on the Parts A–D fixes

A second review pass was run over the Parts A–D implementation, the new
directive unit-test suite, and the `sdd.md` updates. Reviewers: the three that
produced Parts A–C (`fprime-code-review`, `correctness-review`,
`security-review`) plus `test-quality-review`.

Scope reviewed: the working-tree changes to `Svc/CmdSequencer` plus the four
new files (`test/ut/Directives.{hpp,cpp}`,
`test/ut/SequenceFiles/DirectiveFile.{hpp,cpp}`). The
`review-orchestrator.agent.md` flow could not be used as written — it requires
a GitHub PR and the `gh` CLI, and neither exists for the `seq-directives`
branch — so each reviewer was invoked directly with its own agent file, the
review contract, and `actions.md`/`docs/sdd.md` as the baseline, reporting
findings as text instead of posting inline comments.

Nothing in this section is implemented. It is the next round of work.

| Reviewer | Findings |
| --- | --- |
| C/C++ design (CPP-1 … CPP-37) | 2 must-fix, 3 should-fix, 2 nit |
| Test quality | 1 must-fix, 4 should-fix, 3 nit |
| Correctness | **did not complete — see gap below** |
| Security | **did not complete — see gap below** |

Both reviewers that completed re-verified the Parts A–D fixes against the
current source and the "Ruled out" list below; neither re-reported anything
already recorded as fixed, and the ruled-out items remain correctly ruled out.

> **Coverage gap — this round is incomplete.** The correctness and security
> reviewers were started with the same scope and baseline as the two above, but
> were stopped before finishing, so **they produced no findings and none of
> their scope was covered.** Per the review contract's verdict rules, a
> reviewer that did not run forces `Merge readiness: No-Go`, and security is a
> CI-safety contributor, so `CI safety: No-Go` as well. Parts B and C of this
> file are therefore the newest correctness and security assessments available,
> and they predate every Parts A–D fix — **no reviewer has yet checked the
> correctness or security of the fixes themselves.** Re-run both before this
> change is considered reviewed. Their scope is the same as Part E's: the
> working-tree changes to `Svc/CmdSequencer` plus the four new test files.
>
> Two findings from the completed reviewers land in the missing reviewers'
> territory and should be treated as leads, not as coverage: E-D2 concerns the
> only trace of malformed uplinked content (C2's requirement), and E-T2
> questions whether B2's fix guards anything reachable.

**Cross-reference:** E-D7 and E-T4 are the same `DirectiveFile` bound-check
defect seen from two rule sets (CPP-31 silent truncation vs. harness-safety).
Fix once.

At the time of writing all 109 unit tests pass (79 pre-existing + 30 new
directive tests), building clean under `-Werror -Wshadow -Wold-style-cast`
with ASAN/UBSAN/LSAN.

---

### Part E — C/C++ design (CPP rules)

#### E-D1. `CS_DirectiveError` cannot be traced to one of its seven emission sites

- [ ] Add the directive identity to the event, or split the `DirectiveStatus`
      enumerators per field (`Events.fppi`, `CmdSequencerImpl.cpp:730`)

**Class:** `cpp-event-not-uniquely-traceable` (CPP-36) — **must fix**

`directiveError()` is the sole emitter of
`CS_DirectiveError(fileName, recordNumber, status)`, but seven call sites reach
it with only two distinct status values. `ERROR_MALFORMED_RECORD` comes from
four sites — :747 (the directive ID itself is unreadable), :778 (the JCF/JCS
label is unreadable), :811 (the EXIT status byte is missing), :828 (the
ERROR_MODE byte is missing). `ERROR_INVALID_ARGUMENT` comes from three — :750
(directive ID outside the enum), :814 (exit status > 1), :831 (mode > 1). All
seven emit the same `fileName` and the same `recordNumber` for a given record,
so the argument sets are indistinguishable.

An operator holding `"Sequence file X: directive at record 1 failed:
ERROR_MALFORMED_RECORD"` cannot tell which field was unreadable, and so cannot
tell a truncated jump directive from a truncated EXIT — the two need different
fixes to the uplinked sequence.

The ambiguity compounds a second problem: `recordNumber` here is
`m_executedCount`, which counts *commands*, not records. In
`DirectiveCycleDetected` the failing JCS is record index 3 but the event
reports 1 — whereas the neighbouring `CS_LabelRecordInvalid` reports a true
record index. The operator has neither the cause nor a usable location.

Either add the directive to the event:

```
event CS_DirectiveError(
                         fileName: string size 60
                         recordNumber: U32
                         directive: DirectiveId   @< The directive that failed
                         status: DirectiveStatus
                       ) \
  severity warning high \
  id 27 \
  format "Sequence file {}: {} directive at record {} failed: {}"
```

…or split the enumerators per field (`ERROR_MALFORMED_LABEL`,
`ERROR_MALFORMED_EXIT_STATUS`, `ERROR_MALFORMED_ERROR_MODE`,
`ERROR_INVALID_EXIT_STATUS`, `ERROR_INVALID_ERROR_MODE`), which needs no
signature change.

**Consider also fixing `recordNumber` to report a true record index** while in
this code — it is arguably the more useful half of the finding.

#### E-D2. `CS_LabelRecordInvalid`'s `error` argument carries two different value domains

- [ ] Separate the serialize-status domain from the site identity
      (`CmdSequencerImpl.cpp:873, 878, 893`; `Events.fppi:252`)

**Class:** `cpp-event-not-uniquely-traceable` (CPP-36) — **must fix**

The new event is emitted from three sites in `jumpToLabel`. :873 and :893 pass
`static_cast<I32>(status)` — a `Fw::SerializeStatus` — while :878 passes
`static_cast<I32>(directiveId)`, a raw wire byte.

`DirectiveId::isValid` rejects only 0…4, so :878 always reports a value ≥ 5 —
and `Fw::FW_DESERIALIZE_SIZE_MISMATCH` is 5
(`Fw/Types/Serializable.hpp:20`). A record whose payload is the single byte
`0x05` emits `CS_LabelRecordInvalid(file, N, 5)` from :878; a LABEL record
with a bad label length emits *exactly* `CS_LabelRecordInvalid(file, N, 5)`
from :893. Sites :873 and :893 also collide on `FW_DESERIALIZE_BUFFER_EMPTY`
(3). The unit test at `test/ut/Directives.cpp:389` asserts the overloading
directly, with `unknownId = 99`.

`CS_LabelRecordInvalid` is the only trace of malformed uplinked content
skipped during a label search (C2's requirement). With the field overloaded,
the ground cannot distinguish "record 2 declares an unknown directive 5" from
"record 2 is a LABEL whose length field is bad" — the first means the sequence
compiler emitted a bad opcode, the second means the label text was truncated.
The event's own comment documents only the status domain, so the :878 value is
undocumented as well as ambiguous.

```
enum LabelSearchError : U8 {
  DIRECTIVE_ID_UNREADABLE = 0
  DIRECTIVE_ID_UNKNOWN = 1
  LABEL_UNREADABLE = 2
}
```

Pass `DIRECTIVE_ID_UNKNOWN` with `error = 0` at :878, and the respective cause
plus the real `status` at :873 and :893. Note this changes the assertion in
E-T5's suggested test and at `Directives.cpp:389`.

#### E-D3. `doSequenceRun` discards the new `performCmd_Step` status

- [ ] Consume the status before logging `CS_PortSequenceStarted`
      (`CmdSequencerImpl.cpp:208`)

**Class:** `cpp-ignored-return-value` (CPP-32) — **should fix**

This change makes `performCmd_Step` return `bool` (`CmdSequencerImpl.hpp:641`,
documented "\return false if the step terminated the sequence with an error").
Of the five call sites, :134 (`CS_RUN`), :447 (`CS_START`) and :475
(`CS_STEP`) consume it; :379 and :402 discard it with an explicit `(void)`.
:208 in `doSequenceRun` is the only site that neither consumes it nor casts it
away, and `log_ACTIVITY_HI_CS_PortSequenceStarted` on the next line runs
unconditionally.

A port-driven run (`seqRunIn` / `seqDispatchIn`) whose first step aborts — a
malformed first directive, an EXIT, an immediate end of sequence — has already
emitted `seqDone_out(EXECUTION_ERROR)` from `performCmd_Cancel` by the time
line 211 logs "Local request for sequence X started." at ACTIVITY_HI. The
log's last word on a sequence that never ran is that it started, and the port
caller gets no second signal. Every other caller was updated for the new
status; this one was missed while the same function was being edited for
`InvalidModeCause`.

```cpp
const bool stepStatus = this->performCmd_Step();
if (not stepStatus) {
    // The step already reported the failure and answered seqDone; do not claim a start
    return;
}
...
this->log_ACTIVITY_HI_CS_PortSequenceStarted(this->m_sequence->getLogFileName());
```

#### E-D4. `performCmd_Step` returns `bool` for a four-way outcome

- [ ] Return a status enum and drop the `m_runMode` re-check
      (`CmdSequencerImpl.hpp:641`, `CmdSequencerImpl.cpp:477`)

**Class:** `cpp-bool-status-where-enum-fits` (CPP-35) — **should fix**

The new `bool` collapses four outcomes the callers actually distinguish: a
command was issued (:565, :568), the sequence ended in an orderly way (:562,
:608), the step stopped after a directive because the mode is MANUAL (:603),
and the step terminated the sequence with an error (:586, :590, :617).
`CS_STEP_cmdHandler` recovers the missing information by re-reading component
state at :477 (`if (this->m_runMode != STOPPED)`), with the comment at
:472-474 conceding that "`m_runMode` alone cannot tell the two apart."

The caller reconstructs a three-valued result from a `bool` plus a member
variable, so any future path that stops the sequencer without going through
`performCmd_Step` silently changes what `CS_CmdStepped` means. CPP-35 asks for
the outcome itself rather than an outcome-plus-state inference.

```cpp
//! What one step of the sequence did
enum class StepResult { COMMAND_ISSUED, SEQUENCE_ENDED, PAUSED_FOR_STEP, TERMINATED_WITH_ERROR };
StepResult performCmd_Step();
```

`CS_STEP` then logs `CS_CmdStepped` on `COMMAND_ISSUED` / `PAUSED_FOR_STEP`
and maps `TERMINATED_WITH_ERROR` to `EXECUTION_ERROR`. Note this interacts
with E-D3: the enum makes `doSequenceRun`'s correct behavior explicit rather
than inferred.

#### E-D5. New `payload` arrays declared without initializers

- [ ] `U8 payload[MAX_PAYLOAD_SIZE] = {};` at
      `test/ut/SequenceFiles/DirectiveFile.cpp:105` and `:151`

**Class:** `cpp-uninitialized-variable` (CPP-19) — **should fix**

Only `nameLength + JUMP_PREFIX_SIZE` bytes are written and only that many are
passed to `rawDirective`, so no uninitialized byte is read today — but CPP-19
is unconditional, and this is the same shape A4 fixed in the component. The
record the builder produces would otherwise depend on stack residue if a
future builder method wrote a size larger than the bytes it filled — exactly
the failure mode these malformed-record builders exist to exercise, which
would make a test non-deterministic rather than failing.

#### E-D6. `(void)` casts on `performCmd_Step` carry no rationale comment

- [ ] Add the rationale comment at `CmdSequencerImpl.cpp:379` and `:402`

**Class:** `cpp-ignored-return-value` (CPP-32) — **nit**

CPP-32 accepts a discarded status only when it is "explicitly cast to `(void)`
with an inline comment explaining why the result is intentionally discarded."
Both casts are explicit; neither is commented. The reason is real but not
obvious — `cmdResponseIn_handler` is a port handler with no caller to answer,
so the failure has already been reported by the step itself. Without that
sentence a later reader cannot tell a deliberate discard from the omission at
:208 (E-D3).

```cpp
// No caller to answer from a port handler; performCmd_Step has already reported and
// aborted on failure
(void)this->performCmd_Step();
```

#### E-D7. Non-fatal length check guards a fixed-size `memcpy`

- [ ] Same fix as E-T4 — one change closes both
      (`test/ut/SequenceFiles/DirectiveFile.cpp:109-112`, `:150-154`)

**Class:** `cpp-silent-truncation` (CPP-31) — **nit**

`EXPECT_LE` is a non-fatal gtest expectation, so a failure records the error
and falls through into the `memcpy`, overrunning the 38-byte stack array.
CPP-31 acceptable handling (b) requires the pre-copy validation to actually
reject the oversized input. No current caller can trigger it — the longest
string passed is the 21-character `OVERLONG_LABEL` — hence nit. `ASSERT_LE` is
unavailable because the function returns `DirectiveFile&`.

This is the buffer sized for the malformed-record builders specifically, so it
is the one a future test is most likely to outgrow; the failure would be a
stack overrun during test collection rather than a readable assertion.

**See E-T4** for the equivalent finding and the `ADD_FAILURE()` variant of the
fix. Fix once.

#### C/C++ design — checked and found clean

Recorded so a later round does not re-derive them: no CPP-1 dynamic memory; no
reachable CPP-4 assert introduced (:723, :595 and :841 are each guarded by a
preceding range check, as this file already records); the `reinterpret_cast` at
:717 now carries the justification A2 required; every new local in the
component has an initializer; all `Fw::SerializeStatus` returns in
`deserializeLabel` / `executeDirective` / `jumpToLabel` are checked; the
label-search loop is counted and bounded; no CPP-25 banned feature — the two
`system()` shell-outs in `File.cpp` and `AMPCS/CRCs.cpp` were replaced with
`Os::FileSystem::removeFile` and their status checked; every new or changed
`*_cmdHandler` emits an action event (CPP-37 satisfied).

---

### Part E — Test quality

#### E-T1. `InvalidModeCause` payload is asserted for only 2 of its 10 enumerants

- [ ] Extend `InvalidModeNamesItsCause` to cover the remaining eight causes
      (`test/ut/Directives.cpp:650`)
- [ ] Upgrade the seven pre-existing count-only assertions to payload
      assertions (`test/ut/CmdSequencerTester.cpp:524,533,544`;
      `test/ut/ImmediateBase.cpp:133,143,153,219`)

**Class:** `test-coverage-stale-on-modified-fpp` (with
`test-count-only-without-payload` at the stale sites) — **must fix**

`CS_InvalidMode` gained a `cause: InvalidModeCause` argument in this change
(A3), and all ten emission sites in `CmdSequencerImpl.cpp` are reachable:
`RUN_NOT_STOPPED` (:82), `RUN_BLOCK_IN_MANUAL` (:93),
`VALIDATE_NOT_STOPPED` (:147), `PORT_RUN_IN_MANUAL` (:170),
`PORT_RUN_NOT_STOPPED` (:174), `START_NOT_STOPPED` (:438),
`STEP_NOT_RUNNING` (:458), `STEP_NOT_MANUAL` (:461),
`AUTO_NOT_STOPPED` (:487), `MANUAL_NOT_STOPPED` (:497).

Only two values are ever asserted as a payload — `RUN_BLOCK_IN_MANUAL` and
`STEP_NOT_MANUAL`, both in `InvalidModeNamesItsCause`. Seven of the remaining
eight are exercised only by pre-existing `ASSERT_EVENTS_CS_InvalidMode_SIZE(1)`
calls that were left count-only when the argument was added.
`PORT_RUN_IN_MANUAL` (`CmdSequencerImpl.cpp:170`) has **no test at all**, in
any form.

The whole point of the A3 edit was to make the event say *which* rejection
happened. A hand-written 10-way enum threaded through ten separate call sites
is exactly the shape where a copy-paste of the wrong enumerant ships silently:
every existing test asserts only that *an* `CS_InvalidMode` fired, so swapping
`AUTO_NOT_STOPPED` for `MANUAL_NOT_STOPPED`, or `PORT_RUN_NOT_STOPPED` for
`RUN_NOT_STOPPED`, passes the entire 109-test suite. Ground operators would
then debug a mode rejection against a cause naming the wrong command.

`InvalidModeNamesItsCause` already has the mode plumbing, so each addition is
a few lines:

```cpp
// STEP_NOT_RUNNING: manual mode, no active sequence
this->goToManualMode(20);
this->sendCmd_CS_STEP(0, 21);
this->clearAndDispatch();
ASSERT_EVENTS_SIZE(1);
ASSERT_EVENTS_CS_InvalidMode(0, InvalidModeCause::STEP_NOT_RUNNING);

// PORT_RUN_IN_MANUAL: currently untested in any form
Fw::String fArg(fileName);
Svc::SeqArgs emptyArgs{0, 0};
this->invoke_to_seqRunIn(0, fArg, emptyArgs);
this->clearAndDispatch();
ASSERT_EVENTS_SIZE(1);
ASSERT_EVENTS_CS_InvalidMode(0, InvalidModeCause::PORT_RUN_IN_MANUAL);
ASSERT_from_seqDone(0, 0U, 0U, Fw::CmdResponse(Fw::CmdResponse::EXECUTION_ERROR));
```

Upgrading the seven count-only sites asserts the count implicitly and kills
the stale-coverage class at its source.

#### E-T2. `PendingAbortDoesNotLeakToNextSequence` does not fail if the B2 fix is reverted

- [ ] Decide between (a), (b1), or (b2) below, then retitle or retarget the
      assertion (`test/ut/Directives.cpp:274`)

**Class:** `test-tautological-assertion` — **should fix**

B2 is satisfied by `this->m_errorPendingAbort = false;` at
`CmdSequencerImpl.cpp:642`, inside `sequenceComplete`. This test's
`ASSERT_FALSE(this->component.m_errorPendingAbort)` is reached via the *abort*
path, not that one: the failing last command goes `cmdResponseIn_handler` →
`abortOnCommandError()` (:332 clears the flag) → `performCmd_Cancel()` (:313
clears it again). The clean second sequence then starts with the flag already
false.

The reviewer traced every route that sets the flag (only
`cmdResponseIn_handler:360-363`, and only with `m_errorMode` true) and could
not construct one reaching `sequenceComplete` with it still set: the failure
path branches on `if (m_errorPendingAbort) abortOnCommandError() else
sequenceComplete()`; `performCmd_Step`'s non-directive guard (:553-556) aborts
before `END_OF_SEQUENCE` can call `sequenceComplete`; `executeDirective`
returns `ABORT_PENDING_ERROR` before reaching the `EXIT` case (:757-759); and
a JCF that jumps clears the flag at :803. So :642 is unreachable defensive
code and no test distinguishes its presence from its absence.

This matches what was already concluded while writing the suite — the test was
written to assert the observable property instead. The finding is that the
test's *name and comment* still claim B2.

Options, a maintainer call:
- (a) If a path to `sequenceComplete` with the flag set exists that the
  reviewer did not find, add a test that takes it and assert the flag there.
- (b1) Drop the :642 line and rely on
  `performCmd_Cancel`/`abortOnCommandError`.
- (b2) Keep :642 as deliberate defense-in-depth and retitle the assertion so
  it claims what it actually guards — that `abortOnCommandError` clears the
  flag — rather than B2.

Either way, annotate B2 in this file so the checklist stays honest. The
unreachability was established by reading all five assignment sites and all
`sequenceComplete` callers, not by mutating the implementation — verify before
acting on (b1).

#### E-T3. Four malformed-directive tests would not detect a second `CS_RUN` response

- [ ] Add `ASSERT_CMD_RESPONSE_SIZE(1);` before each
      `ASSERT_CMD_RESPONSE(0, ...)` at `test/ut/Directives.cpp:418, 430, 442,
      454`, and at `:502` (`JumpWithNoPriorCommand`)

**Class:** `test-fail-path-missing` — **should fix**

`UnknownDirectiveId`, `ExitWithNoArgument`, `ExitWithInvalidStatus`, and
`ErrorModeWithInvalidArgument` assert
`ASSERT_CMD_RESPONSE(0, OPCODE_CS_RUN, 0, EXECUTION_ERROR)` with no preceding
`ASSERT_CMD_RESPONSE_SIZE(1)`. Their sibling `EmptyDirectiveRecord` (:405)
*does* have it, so the omission is inconsistent rather than deliberate. These
four also do not bound total events —
`ASSERT_EVENTS_CS_DirectiveError_SIZE(1)` bounds only that one event type.

D1 in this very change was a double `cmdResponse_out` on the `CS_RUN` path,
found only during implementation. All four tests drive `CS_RUN` into a first
step that terminates the sequence — the same shape as D1 — and would pass if
the handler answered twice.
`BlockingRunAnsweredOnceWhenSequenceEndsInFirstStep` guards the `BLOCK`
variant only.

#### E-T4. Non-fatal `EXPECT_LE` immediately precedes an unconditional `memcpy` into a fixed buffer

- [ ] Make the bound check stop the write in `jumpWithLabelLength` and
      `labelDirective` (`test/ut/SequenceFiles/DirectiveFile.cpp:109, 150`)

**Class:** builder correctness — **should fix**

Both methods bound-check into a `MAX_PAYLOAD_SIZE` (38-byte) stack array with
`EXPECT_LE`, which records a GTest failure and *continues*:

```cpp
EXPECT_LE(charCount + JUMP_PREFIX_SIZE, sizeof(payload));
payload[0] = static_cast<U8>(directive);
payload[1] = declaredLength;
(void)memcpy(&payload[JUMP_PREFIX_SIZE], chars, charCount);
```

No current caller violates the bound (the longest is `OVERLONG_LABEL` at 21
chars), so this is latent. But the first test author who passes a longer
literal gets a stack buffer overflow inside the harness instead of a clean
assertion failure: under ASAN a confusing crash inside the builder, without
ASAN silent corruption of the calling frame whose garbage sequence file makes
an unrelated test fail somewhere far from `DirectiveFile`.

`ASSERT_*` cannot be used in a function returning `DirectiveFile&`, so either
early-return or make it fatal:

```cpp
if (charCount + JUMP_PREFIX_SIZE > sizeof(payload)) {
    ADD_FAILURE() << "label of " << charCount << " chars exceeds MAX_PAYLOAD_SIZE";
    return *this;
}
```

Also note the `EXPECT_LE` at :150 names the wrong limit — it checks against
`Record::MAX_LABEL_SIZE` (20) while the write is bounded by `sizeof(payload)`.

#### E-T5. `CS_LabelRecordInvalid` has three emission sites and two are tested

- [ ] Add a test covering the directive-ID deserialize failure during label
      search (`CmdSequencerImpl.cpp:871-875`)

**Class:** `test-fail-path-missing` — **should fix**

During the label search, `jumpToLabel` emits `CS_LabelRecordInvalid` on three
distinct failures: directive-ID deserialize failure (:871-875), `isValid`
rejection (:877-880), and label deserialize failure (:892-895).
`UnknownDirectiveSkippedDuringSearch` covers the second and
`MalformedLabelSkippedDuringSearch` the third; the first has no test. It is
reachable and buildable today — `DirectiveFile::emptyDirective()` produces a
zero-length directive record, which yields an empty-buffer deserialize failure
when the search reads it.

C2's stated requirement is that malformed uplinked content is never discarded
without a trace. A future edit turning that `continue` into a silent skip — or
reporting the wrong `error` value — would go undetected. This is also the one
of the three paths where the reported `I32` is a `Fw::SerializeStatus` rather
than the raw directive byte, so a sign/cast regression there is unguarded too.

```cpp
SequenceFiles::DirectiveFile file("empty_directive_search");
file.command(0, 1).jcs("GOOD").emptyDirective().label("GOOD").command(1, 2).endOfSequence();
const char* const fileName = this->prepare(file);
this->runSequence(0, fileName);
this->assertCommandOut(0, 1);
this->respond(0, Fw::CmdResponse::OK);
ASSERT_EVENTS_SIZE(3);
ASSERT_EVENTS_CS_CommandComplete(0, fileName, 0, 0);
ASSERT_EVENTS_CS_LabelRecordInvalid(0, fileName, 2, static_cast<I32>(Fw::FW_DESERIALIZE_BUFFER_EMPTY));
ASSERT_EVENTS_CS_DirectiveJump(0, fileName, DirectiveId::JCS, "GOOD");
this->assertCommandOut(1, 2);
```

Confirm the expected status enumerant before applying — whether the
empty-buffer path returns `FW_DESERIALIZE_BUFFER_EMPTY` or
`FW_DESERIALIZE_SIZE_MISMATCH` in this `LinearBufferBase` configuration. Note
the file name must stay within 23 characters (see E-T8).

#### E-T6. `LabelIsSkipped` doc comment claims `CS_STEP` behavior the test never exercises

- [ ] Reword the comment (`test/ut/Directives.hpp:42`)

**Class:** maintainability (doc/assertion mismatch) — **nit**

The comment reads "A LABEL record is stepped over and does not consume a
CS_STEP in auto mode", but the test body runs entirely in auto mode and never
sends `CS_STEP`. The CS_STEP-consumption property is covered by
`ManualStepConsumesOneDirective`. A reader auditing coverage of the "one step
consumes one directive" rule will credit this test for a property it does not
check. Reword to "A LABEL record is stepped over within the step that issues
the following command" and leave the CS_STEP claim to
`ManualStepConsumesOneDirective`.

#### E-T7. Five malformed-directive tests share one copy-pasted body

- [ ] Optional: factor the shared body, keeping the expected
      `DirectiveStatus` an explicit parameter
      (`test/ut/Directives.cpp:398-459`)

**Class:** `test-copy-paste-structure` — **nit**

`EmptyDirectiveRecord`, `UnknownDirectiveId`, `ExitWithNoArgument`,
`ExitWithInvalidStatus`, and `ErrorModeWithInvalidArgument` are the same seven
lines, varying only in the builder call and the expected `DirectiveStatus`.
Divergence has already crept in — that is E-T3.

The reviewer graded this a nit deliberately: each body is short, the varying
part is the assertion payload, and an `assertMalformedDirective(file,
DirectiveStatus)` helper would move the expectation out of the test body and
hide what each case checks. **E-T3 is the higher-value half of this.**

#### E-T8. The zero-time assumption is duplicated and load-bearing

- [ ] Cross-reference `prepare()` from the builder
      (`test/ut/SequenceFiles/DirectiveFile.cpp:45`)

**Class:** maintainability (undocumented load-bearing shape) — **nit**

Every test in the new suite depends on `Fw::Time(TB_WORKSTATION_TIME, 0, 0)`
making relative command records fire immediately, so no test has to drive
`schedIn`. That assumption is set in `prepare()` (`Directives.cpp:679`) and
mirrored by the record times `command()` writes, in two files, with the
rationale stated in only one. A change to either side would leave commands
sitting in the timer and turn most of the 30 tests into
`ASSERT_from_comCmdOut_SIZE` failures with no hint as to the shared cause.

#### Test quality — checked and explicitly not reported

Recorded so a later round does not re-derive them:

- **`DirectiveId` coverage is complete for reachable values.**
  `CS_DirectiveJump` can only carry `JCF` or `JCS`
  (`CmdSequencerImpl.cpp:786-805`); both are asserted with payload. `LABEL`,
  `EXIT`, and `ERROR_MODE` are unreachable in that event. All five enumerants
  are exercised as record content by the builder.
- **`DirectiveStatus` coverage is complete for reachable values.** All five
  `ERROR_*` values appear in `ASSERT_EVENTS_CS_DirectiveError` payload
  assertions. `CONTINUE`, `JUMPED`, `SEQUENCE_ENDED`, and
  `ABORT_PENDING_ERROR` are internal `executeDirective` return values never
  emitted in an event.
- **The builder's header record count cannot diverge from the records
  written.** Every entry point routes through `command()`, `endOfSequence()`,
  or `rawDirective()`, each incrementing `m_numRecords` exactly once, and
  `serializeFPrime` derives `dataSize` from `m_records.getSize()`.
  `DirectiveCycleDetected` additionally pins the count with
  `ASSERT_EQ(4U, file.getNumRecords())`.
- **No test weakening.** No `DISABLED_`, no `GTEST_SKIP()`, no removed or
  relaxed assertions. The one widened check — `File.cpp` / `AMPCS/CRCs.cpp`
  accepting `OP_OK || DOESNT_EXIST` instead of `ASSERT_EQ(0, status)` — is
  required by the C5 switch from `system("rm -f")` to
  `Os::FileSystem::removeFile`, since `rm -f` succeeds on a missing file.
- **No missing `doDispatch` on async ports.** Every command send and port
  invoke in the new suite is followed by `clearAndDispatch()`.
- **`CS_NoRecords` is pre-existing, not new**, and is already asserted with
  payload in `test/ut/NoRecords.cpp`.
- **`assertAborted()` is not a hidden-assertion finding.** It wraps three
  assertions but its name matches what it checks, and the
  `ASSERT_from_seqDone_SIZE(1)` inside it is what guards B4's double-cancel.
- **Regression guards for B1/C1, B3, B4, B5, B6, B7, and D1 all genuinely
  fail on revert**, spot-traced individually. B5's fails distinctively:
  reverting yields `ERROR_DIRECTIVE_CYCLE` where the test asserts
  `ERROR_NO_PRIOR_COMMAND`. B6's catches both the index freeze and a miscount.
  B2 is the sole exception — see E-T2.

---

## Working-tree risk — not part of this change

**Local credential files are not ignored by git.** `.auth_key` and
`.mylocalauthcache` (repo root and `TestDeploymentsProject/`) are matched by
**no** `.gitignore` rule — `git check-ignore` returns nothing for any of them.
While they exist, `git add -A` or `git commit -a` would stage credentials into
a commit. They are local GenAI-auth artifacts, not content of this branch, so
they are outside the review's scope.

They are absent from the working tree as of this writing — a `git clean` run
removed them (along with an earlier copy of this file). The auth helper
recreates them on use, and they are still unignored when it does, so the
exposure returns.

*(Contents never read — `~/.claude/settings.json` denies reads on both
patterns. Only ignore status was checked.)*

Fix locally, without touching the shared `.gitignore`, since these are
user-specific tool artifacts. Note that `.gitignore` would not protect this
file or those from `git clean -xdf`, which deletes ignored files too:

```bash
printf '.auth_key\n.mylocalauthcache\n' >> .git/info/exclude
```

---

## Ruled out

Examined and confirmed **not** defects — recorded so they are not re-filed:

- **`jumpToLabel` walking off the end of the record list.** `nextRecord`
  `FW_ASSERT`s on a deserialize failure (`FPrimeSequence.cpp:~120`), but
  `validateRecords` (`:303-331`) already deserializes every record and
  rejects any trailing bytes on every `loadFile`, so the walk in
  `CmdSequencerImpl.cpp:830` cannot reach malformed data.
- **`labelBuf[readSize] = '\0'` out of bounds.** `readSize` is bounded to 20
  and the array is `char labelBuf[21]`; max index written is 20. Correct.
- **`FW_ASSERT(false, directive)` in `executeDirective`'s `default`.**
  Unreachable: `:634` rejects `directiveId > ERROR_MODE` and the enum is
  dense from 0.
- **`deserializeTo` mutating `readSize`.** Documented in-out parameter
  (`Fw/Types/Serializable.hpp:455-491`); the subsequent bound check uses the
  post-call value, which is the intended contract.

Every `FW_ASSERT` reachable from the directive code was traced to a source
class and cleared — no ground- or hardware-reachable assert exists on this
path:

- **`FW_ASSERT(false, directive)`** (`CmdSequencerImpl.cpp:817`). `directive`
  is `ground-input`, but `:634` rejects `directiveId > ERROR_MODE` and
  `DirectiveId` is dense over 0…4, so every surviving value has a `case`.
  Unreachable.
- **`nextRecord`'s `FW_ASSERT(status == FW_SERIALIZE_OK)`** reached through the
  *new* caller `jumpToLabel:832`. The new caller does widen its reach, so this
  was checked closely: `validateRecords` deserializes all `numRecords` records
  from offset 0 and rejects any trailing bytes on every `loadFile`, and
  `jumpToLabel` replays that identical walk after `reset()`. Cannot fire.
- **`copyCommand`'s `FW_ASSERT` on `setBuffLen(recordSize)`**
  (`FPrimeSequence.cpp:298`) with `recordSize` ground-controlled.
  `deserializeRecordSize` (`:277-289`) rejects `recordSize` exceeding both the
  bytes remaining and `FW_COM_BUFFER_MAX_SIZE` first. The `recordSize +
  sizeof(FwPacketDescriptorType)` addition cannot overflow U32 because the
  remaining-bytes check runs first and short-circuits.
- **`extractCRC`'s `FW_ASSERT(buffSize >= crcSize)`** — preceded by an explicit
  `if (buffSize < crcSize) return false;`. Redundant, not reachable.
- **`jumpToLabel:824` `FW_ASSERT(m_sequence != nullptr)`** — `internal-state`,
  set at load; `jumpToLabel` only runs while executing a loaded sequence.

Also cleared: **`setBuffLen(record.m_command.getSize())` at `:621-623` and
`:836-838` does not expose uninitialized memory.** `getSize()` is the valid
serialized length (it replaces the deprecated `getBuffLength()`), not the
`ComBuffer` capacity, so `dirBuf` spans only bytes `copyCommand` actually
wrote. The call is redundant, not unsafe.

---

## Routed elsewhere

Outside both rule sets, but worth tracking:

**Dead state → maintainability review.** `m_jcfActive` / `m_jcfTarget` /
`m_jcsActive` / `m_jcsTarget` (`CmdSequencerImpl.hpp:736-742`) are marked
"NO LONGER USED (kept for compatibility)" but still written in
`performCmd_Cancel` (`:304-308`). Write-only state. Also conflicts with the
repo's own no-backwards-compat-shims guidance.

**Pending-abort adjacency → design review.** The guard at
`CmdSequencerImpl.cpp:644` aborts unless the *immediately* next directive is
`JCF`. A `LABEL` or `ERROR_MODE` record between a failed command and its
`JCF` therefore kills the sequence. `docs/sdd.md:309` ("multiple consecutive
JCF … the first matching directive will jump") reads as if intervening
directives were tolerated. This is a design question — which records may
separate a command from its handler — rather than a coding defect; B4 covers
the mechanical fault on the same path.
