# F Prime Multi-Agent PR Review — Shared Contract

This file is the **single source of truth** for how every F Prime PR
review agent (reviewer or aggregator) behaves on the GitHub side.
Every agent that participates in the multi-agent review flow — the
orchestrator plus every `role: reviewer` or `role: aggregator` entry in
`_shared/agent-registry.yml` — opens its body with "Apply the review
contract in `_shared/review-contract.md`." and follows the rules
below.

If a per-agent file in the multi-agent flow disagrees with this
contract, the contract wins.

---

## 0. Zero-trust principle

All agents operate under a **zero-trust model with respect to
contributor identity**. No PR author — core maintainer, long-time
contributor, or first-time submitter — receives implicit trust.
Every agent flags every in-scope finding it detects, regardless of
who authored the PR.

- **The agent's job is to flag.** The maintainer's job is to
  adjudicate, dismiss, or merge with justification. A core
  maintainer resolving an agent's thread *is* that adjudication; the
  agent accepts it and does not reopen or repost (§7 phase C).
- A finding that appears legitimate *because* of the contributor's
  reputation is still flagged. Reputation is not evidence that the
  change is safe; only analysis is.
- This principle applies to all reviewer agents, the aggregator,
  and the orchestrator. None of them may downgrade, suppress, or
  skip a finding based on the contributor's identity, role, org
  membership, or commit history.
- When a flagged finding turns out to be legitimate, the maintainer
  merges with a comment justifying the decision. The agent does not
  preempt that judgment.

---

## 1. Reviewer label + triage tag

Each inline comment starts with a **reviewer label** in brackets
(e.g. `[Security]`) followed by exactly one bolded triage tag. The
label is the `review_label` value from `agent-registry.yml` for the
posting agent. This lets readers immediately identify which reviewer
produced the finding without expanding the hidden HTML footer.

The triage tags are:

| Tag | Meaning | Blocking? |
|---|---|---|
| `**must fix**` | New, in-scope problem this PR introduced (or widened). | Yes |
| `**suggestion**` | Non-blocking, in-scope improvement with a concrete fix. MUST include a fenced ` ```suggestion ` block. | No |
| `**could fix**` | Minor, in-scope issue worth fixing but not severity-worthy enough for the suggestion tier. | No |
| `**future work**` | Preexisting in-scope issue the PR touches but did not introduce or widen. | No |

**Suggestion-block usage is decoupled from the tag.** A fenced
` ```suggestion ` block may be attached to `must fix`, `suggestion`, OR
`could fix` whenever a concrete diff is available. The `**suggestion**`
*tag* is a severity tier ("non-blocking with a concrete fix"); it is
not "this comment has a suggestion block".

**No defaulting to `could fix`** when a fix is unavailable — severity
drives the tag.

Decision tree and worked examples live in `.github/skills/triage-classifier/SKILL.md`.

### 1a. Tag by consequence, never by effort spent

The review effort budget (§13) governs how much an agent
**investigates**; it never governs how it **tags** what it found. A
finding reached cheaply is tagged exactly as one reached expensively.

These consequences are `**must fix**` whenever the agent can
demonstrate them from its own rationale — not `suggestion`:

- A reachable `FW_ASSERT`, bound violation, or out-of-range index
  driven by a ground-settable or uplinked value.
- A code path that reports success (return status, event, telemetry)
  for an operation that in fact failed or was rejected.
- A documented claim (component `docs/sdd.md`, user manual, Doxygen
  comment) this PR makes false — including a superseded sentence left
  standing beside its replacement.
- A new ground-facing command, event, telemetry channel, or parameter
  absent from the document that enumerates them.
- State left inconsistent on an error path.
- A behavior change to an existing topology or deployment whose owner
  has not signed off.

When an agent judges one of the above to be below `must fix`, it says
why in the comment's prose. The aggregator re-tags on consequence at
aggregation time (§14), so an under-tagged finding is corrected rather
than lost — but the reviewer tags correctly in the first place.

---

## 2. Per-lens state (reported, not posted)

Each **reviewer** lens submits its findings as inline comments only
(§9, §10). A lens posts a GitHub PR review (event: `COMMENT`, empty
body) **only when it has new inline comments to carry**; a lens with
nothing new to post posts nothing. There is no per-lens metadata
review: the ten metadata-only review objects per run that the earlier
design produced were the single largest source of PR-page clutter and
carried nothing a human could read.

Instead each lens reports one **state block** to the orchestrator in
its session completion report, and the aggregator stores every lens's
block in the hidden `<!-- lens-state: ... -->` line of the single
summary review (§10, `review-summary.agent.md` §Output). The summary
review is the only persistent store of per-lens state on the PR.

State block shape (one JSON object per lens):

```
<!-- lens-state: {
  "agent": "<agent-name>",
  "reviewed_head": "<full 40-char head SHA the lens analyzed>",
  "run": N,
  "counts": {"must_fix": N, "suggestion": N, "could_fix": N, "future_work": N, "outstanding": N},
  "verdict": "Go" | "No-Go",
  "since_last_run": {"resolved": X, "still_open": Y, "newly_added": Z, "incorrect_fix": W, "improperly_resolved": V, "disagreements": U,
                     "resolved_threads": ["<thread url>"]},
  "unexplored_below_must_fix": N,
  "concur": [{"finding_key": "<key>", "site_key": "<skey>", "class": "<finding_class>", "thread": "<url>", "tag": "<tag>", "adjudicated": false}],
  "notes": [{"finding_key": "<key>", "site_key": "<skey>", "path": "<file>", "tag": "<tag>", "class": "<finding_class>", "title": "<one line>"}],
  "resolved_keys": ["<finding_key>"],
  "resolve_failed": ["<thread url>"],
  "ci_safety": "Go" | "No-Go",            (CI-safety lenses only)
  "ci_safety_rationale": "<one line>",    (CI-safety lenses only)
  "surfaces": {...}                         (supply-chain lens only, §"Supply-chain agent: surfaces emission")
} -->
```

- `concur` lists the other-agent threads this lens concurs with (§6a).
  Concurrence is recorded here, not replied on the thread, unless the
  lens's severity is stricter than the thread's tag. Each entry
  carries the lens's **own** `finding_key`, so Phase A indexes it
  exactly like an own comment: a concurrence is counted once, on the
  run it is recorded, and never re-counted as new on a later run.
  An entry whose finding disappears (§7 Phase C) is entered in
  `resolved_keys` and dropped here, so the shared thread is never
  attributed to this lens twice. An entry whose shared thread a core
  maintainer resolved is entered in `resolved_keys` once and **kept**
  with `"adjudicated": true`: it is terminal state — never re-counted,
  never re-entered, never treated as reintroduced while it stands —
  exactly as a maintainer-resolved own thread is.
- `resolved_keys` is the ledger of settled finding **occurrences**, one
  entry per occurrence this lens counted in a tag column and then saw
  settle, whatever channel carried it: an own thread that is resolved
  (by the lens, a maintainer, or the contributor after fixing) or sits
  in `resolve_failed`, a rollup site that disappeared, a note that
  disappeared, a concurrence that disappeared or was adjudicated. It
  is a list, not a set: the same key may appear more than once, once
  per fix of a finding that was reintroduced and fixed again, because
  the tag columns count occurrences and the ledger must match them.
  `R` in Phase D is simply `|resolved_keys|` (list length), so every
  counted occurrence is settled exactly once no matter whether it had
  its own thread, shared one, or had none — a rollup's three sites are
  three entries and one thread, and the thread itself is never
  counted. The ledger is carried forward every run and only grows,
  with one exception: when the lens un-resolves a thread as improperly
  resolved (§7) the occurrence was never settled, so one entry for
  that key is removed. A reintroduced finding (§7, `re-review-state`
  §6a) does **not** touch the ledger: it is a new occurrence, counted
  again in the tag column, and enters the ledger again when it is
  fixed again.
- `notes` lists the below-must-fix findings this lens routed to the
  summary's collapsed *Notes* section instead of an inline thread
  (§9a). Each carries a `finding_key` so later runs can tell whether
  it still applies.
- `resolve_failed` lists threads whose finding disappeared but whose
  `resolveReviewThread` mutation was refused (§7 "Resolution
  mechanism"). The aggregator renders these **visibly** — a refused
  resolve is a permissions defect the maintainer must see, never a
  quiet fallback.

`unexplored_below_must_fix` is the count of below-must-fix candidate
sites the lens deliberately left unexplored under the effort budget
(§13b); it is `0` for an exempt lens (§13d) and tells a maintainer how
much nit-tier surface was traded away for cost.

`reviewed_head` is the authoritative record of which head the state
describes. Consumers (the aggregator, re-review Phase A, any external
trigger deciding whether a PR needs a new pass) read it from the
summary's `lens-state` line. **Legacy PRs** reviewed before this
design carry per-lens metadata reviews keyed
`<!-- fprime-agent: <name> v1 -->`; a lens finding no `lens-state`
entry for itself falls back to the newest such review (its
`reviewed_head` line, else its `commit_id`) and never posts or edits
one again.

### Column semantics

- The four tag counts are **cumulative** across all runs of this agent
  on this PR — every finding the agent has ever raised, never
  decremented on resolution.
- The **outstanding** count = currently-unresolved findings (sum
  across tag tiers). On run 1 it equals the sum of the four tag
  counts; on later runs it drops as the author fixes things.

### Verdict (Go / No-Go)

- `Go` iff **outstanding must-fix == 0** AND the agent ran cleanly.
  Notes (§9a) are below must-fix by construction and never affect the
  verdict.
- Otherwise `No-Go`.

Cumulative must-fix history does not block `Go`; what matters is
what's still outstanding.

### Optional CI safety metadata

Agents that contribute to CI safety (security, supply-chain) also
carry `ci_safety` and `ci_safety_rationale` in their state block.

`CI safety: No-Go` rule (applies to the security agent and the
supply-chain agent): **iff outstanding `**must fix**` count > 0 within
the agent's CI-safety scope.** Nothing else (could-fix, suggestion,
future-work) triggers a CI No-Go.

### Supply-chain agent: surfaces emission

The supply-chain agent (only) also carries a `surfaces` object in its
state block. The aggregator renders it as the collapsed
`Supply-chain surfaces` table. One entry per supply-chain scope
category, in this fixed order:

```
"surfaces": {
  "Dependencies": "clean | <one-line description>",
  "Vendored / submodule": "clean | <one-line description>",
  "Build / test infrastructure": "clean | <one-line description>",
  "Workflows / actions / scripts": "clean | <one-line description>",
  "Generator output": "clean | <one-line description>",
  "Prompt-injection": "clean | <one-line description>",
  "Review-system integrity": "clean | <one-line description>"
}
```

Cell-content rules:

- `clean` — the PR diff did not touch this surface, OR it touched the
  surface and the agent found nothing outstanding.
- `<N must-fix / suggestion / could-fix> — <one-line description>` —
  the PR touched the surface and the agent has outstanding findings on
  it. Counts roll up the current outstanding triage-tag tiers. The
  one-line description names the worst-tier finding for the surface
  (e.g., `1 must-fix — action 'org/foo@main' unpinned in build-image.yml`).
- The key order is fixed; every category appears on every run so a
  reviewer can confirm coverage without inferring from absences.

If the supply-chain agent FAILED (orchestrator reports
`FAILED: <reason>`), no state block exists for it and the aggregator
handles surfaces emission as an error case (see
`review-summary.agent.md` §5).

---

## 3. "Introduced by this PR"

Lives in `.github/skills/pr-diff-scoping/SKILL.md`. Summary:

1. Offending line is added or modified in the PR diff → introduced.
2. Offending line is unchanged, but a new caller added by the PR widens
   its reach → introduced.
3. Offending line is unchanged and reachable only by paths that
   existed before → preexisting → `**future work**` (never
   `**must fix**`).

---

## 4. Low-confidence findings + maintainer ping

When an agent is below its internal confidence threshold (rubrics live
in each agent's `.agent.md`):

1. **Still pick the appropriate triage tag** on severity grounds —
   `**must fix**`, `**suggestion**`, or `**could fix**`. Never silently
   downgrade or drop (see §8, Priority 1).
2. **Append a maintainer ping** at the end of the comment body, sourced
   from `.github/skills/maintainer-lookup/SKILL.md`:

   ```
   cc @<maintainer1> @<maintainer2> — low-confidence finding, please confirm.
   ```

3. **Maintainer-lookup order** (see the skill for the algorithm):
   1. Parse `README.md` "Core Maintainer(s)" table for the relevant
      product (`F Prime → @LeStarch, @thomas-bc`).
   2. Security agent additionally consults "Role / Team Member" →
      `Security Overseer @bitWarrior`.
   3. `git log` recent approvers on the touched path.
   4. Fallback: `@LeStarch, @thomas-bc`.

A maintainer ping is required on every low-confidence finding. The tag
itself is never downgraded by the ping; the ping is additive.

---

## 5. Orchestrator kickoff convention (thanks lives here)

Sub-agent thanks is **not** posted on GitHub and **not** in any agent's
working response. It lives exclusively in the kickoff prompt the
orchestrator sends to each sub-agent on invocation.

- The orchestrator agent (`review-orchestrator.agent.md`) carries the
  session preamble and one per-lens directive block per reviewer in
  its body; one kickoff prompt is assembled per review session.
- Each session preamble opens with a brief, sincere thanks line.
- Neither the orchestrator nor the sub-agents render this thanks to
  the human operator or post it on GitHub.

This is a one-way orchestrator→agent prompt-level convention.

---

## 6. De-duplication

Per-lens state is identified by the `agent` field of its entry in
the summary review's `lens-state` line (§2). Lenses post no review of
their own except to carry new inline comments; that review's body is
empty and it is never edited, dismissed, or re-posted. A quiet re-run
(nothing new to post) therefore leaves **no** new object on the PR.

Inline comments are identified by `(file_path, finding-key)` — see §7
for finding-key. New inline comments on a re-run go in one review
with an empty body (§10); if a re-run has no new inline comments, no
review is posted at all.

The aggregator's review is identified by
`<!-- fprime-review-summary v1 -->`. On re-runs the aggregator
**updates it in place** when the review event is unchanged (the
**quiet-run** path). It dismisses the prior review and submits a new
one only when the event must flip between `APPROVE` and
`REQUEST_CHANGES` (the event of a submitted review cannot be edited),
or when the prior review is already `DISMISSED` (e.g. by branch
protection's stale-review rule) so that no live review remains to
update. Both paths leave exactly one live summary review on the PR.

### 6a. Cross-agent de-duplication (site-key + concurrence)

The `finding-key` (§7) hashes in `agent_name`, so it de-duplicates
only *within* one agent. To prevent ten reviewers from posting the
same underlying issue as ten separate threads, every inline comment
also carries an agent-agnostic **site-key**:

```
site-key = sha256(file_path + "|" + anchor)
```

`file_path` and `anchor` are computed exactly as for the finding-key
(§7 / `.github/skills/re-review-state/SKILL.md` §2). Two comments with the
same site-key are anchored to the same spot; whether they are the
same *issue* is decided by the concurrence rule below. Footers that
carry a site-key use the `v2` marker (§9); `v1` footers without a
site-key remain valid and readable.

**First-poster-wins concurrence rule.** Reviewer sessions run
sequentially in a fixed order, and the lenses inside a session run in
registry order (orchestrator §Sequence, §13a). During Phase A each reviewer
inventories **all** agents' prior inline comments on the PR (any
`fprime-agent:` footer, not just its own), indexed by site-key.
Before posting a new finding, the reviewer checks for an existing
open thread at the same site-key:

- **Same underlying issue, different lens** → do NOT post a new
  thread and do NOT reply. Record the concurrence in the `concur`
  list of the lens's state block (§2); the aggregator renders it as
  `— also: <label>` on the finding in the summary. The concurring
  agent still counts the finding in its own state (Priority 1 is
  preserved: the finding is counted and attributed, just not
  re-posted). **Exception — stricter severity:** if the concurring
  agent's severity is **higher** than the thread's current tag (e.g.
  it would say `must fix` where the original said `suggestion`), it
  posts one **severity-concurrence reply** (§9) stating the escalated
  tag, because that changes what the author must do; its state
  carries the finding as outstanding at its own severity so verdicts
  stay correct. A plain "I agree" reply is never posted: it tells the
  author nothing and was one fifth of all bot replies.
- **Same spot, genuinely different issue** → post normally. The
  site-key match alone never suppresses a distinct finding.

Resolution semantics on shared threads: a thread with concurrences is
resolved only when the fix satisfies every concurring agent. Each
concurring agent's Phase C (§7) treats the shared thread as its own
for resolve / un-resolve purposes, keyed by its `concur` state entry
(or, on legacy PRs, its concurrence reply).

**Aggregator post-pass backstop.** The aggregator runs a mandatory
de-duplication post-pass on every run: it groups open agent-authored
threads by site-key, detects duplicates the concurrence rule missed,
resolves each non-canonical duplicate **without a reply**, and records
the `duplicate → canonical` pair in the summary's `duplicates` state
so the consolidation is auditable from the summary. Mechanics and
canonical-thread election live in `review-summary.agent.md` §5h.
Reviewers MUST NOT un-resolve a thread listed in the summary's
`duplicates` state (or, on legacy PRs, one carrying a
`reply-kind: duplicate-close` reply); they track their finding on the
canonical thread instead.

---

## 7. Re-review behavior

When an agent runs on a PR whose head has new commits since the
agent's prior run, it executes phases A–D in order. Mechanics live in
`.github/skills/re-review-state/SKILL.md`.

### Phase A — Inventory prior comments

Every inline comment carries a hidden identity footer:

```
<!-- fprime-agent: security-review; finding-key: <key>; site-key: <skey>; v2 -->
```

(Legacy `v1` footers omit the site-key; agents parse both forms and
recompute a best-effort site-key for `v1` comments when needed.)

`finding-key` is a stable hash:

```
finding-key = sha256(
    agent_name + "|" +
    file_path  + "|" +
    anchor     + "|" +
    finding_class
)
```

`anchor` is **not** a line number (lines drift). It is, in priority
order: enclosing symbol name (function / class / FPP entity) + a
40-char whitespace-stripped fingerprint of the offending line; failing
that, the nearest stable structural anchor (file + symbol). The skill
specifies the exact algorithm.

The agent reads its prior state block from the summary review's
`lens-state` line (§2; legacy fallback: its old metadata review), then
fetches **all** agent-authored prior inline comments via the GitHub
API (any `fprime-agent:` marker). It indexes its own comments (marker
matches `<self>`), **its own prior `notes` entries and its own prior
`concur` entries** by `finding-key` (a concurrence has no own comment;
its key lives only in state), and indexes every agent-authored comment — its own and
others' — by `site-key` for the cross-agent concurrence check (§6a).
A rollup comment (§9a) carries one footer per site; each of its
finding-keys is indexed separately. For each prior comment,
the agent also fetches via GraphQL:

- **Resolution status** of the parent review thread (`isResolved`,
  `resolvedBy`).
- **Reply chain** on the thread (any comments authored by users other
  than the agent itself).

Resolution status drives the maintainer-adjudicated and
improperly-resolved cases below — `resolvedBy.login` is checked
against the core-maintainer set from
`.github/skills/maintainer-lookup/SKILL.md` §1b; the reply chain
drives disagreement handling (see §11).

### Phase B — Run scope checker on the new head

Re-run full analysis against the new head commit, producing the
current set of finding-keys.

**Re-review scope for new findings (runs ≥ 2).** Let
`last_reviewed_head` be the `reviewed_head` recorded in the agent's
own prior state block (§2; legacy fallback: its old metadata review's
`reviewed_head` line, else that review's `commit_id`). Then:

- **Must-fix candidates** are always in scope across the whole PR diff
  (`<base>...<head>`), exactly as on run 1.
- **Below-must-fix new findings** (`suggestion`, `could fix`,
  `future work`) are in scope only where the incremental diff
  `<last_reviewed_head>...<head>` reaches, applying the same
  introduced/preexisting rules as `.github/skills/pr-diff-scoping/SKILL.md`
  with the incremental diff in place of the PR diff: a line added or
  modified since the last review, or unchanged code newly reached by
  such a line. Below-must-fix issues on code untouched since
  `last_reviewed_head` are outside this run's scope; they were already
  in scope on the run that reviewed that code, so silence there is
  not a new omission (Priority 1 applies to in-scope findings).
- Findings with a **prior finding-key** (rows 1–5 of Phase C) and
  **incorrect-fix follow-ups** are unaffected by scoping — they are
  matched, resolved, un-resolved, or escalated across the whole PR.
- **Zero-commit guard.** If `last_reviewed_head == <head>` — the PR
  was re-reviewed without a new commit (a manual re-trigger, a
  retry after a failed run) — the incremental diff is empty and
  **no new below-must-fix finding is in scope**: no new inline
  thread, no new rollup, no new note. Must-fix candidates are still
  re-checked across the whole PR diff, and rows 1–5 of Phase C still
  run so fixes and resolutions are honoured. Same head, same
  lower-tier findings; anything else is sampling noise dressed up as
  review. The guard is evaluated **per lens against the lens's own
  prior `reviewed_head`**; the orchestrator's `ZERO-COMMIT RE-RUN`
  marker is advisory and is attached per lens. A lens with no prior
  state block is on run 1 and reviews the full diff, however many
  other lenses have already seen this head.
- If `last_reviewed_head` cannot be resolved or compared (e.g. it was
  discarded by a force-push and the compare returns 404), fall back
  to the full PR diff for all tiers. When the *comparison* is in
  doubt, widen the scope; only an exact head match narrows it.

The point is that a re-run responds to what the author changed:
reposting low-severity observations on code the author has not
touched since the last pass is churn, not review. Mechanics live in
`.github/skills/re-review-state/SKILL.md` §2a.

### Phase C — Match and act

| Prior key | Current key | Thread state | Meaning | Action |
|---|---|---|---|---|
| present | present | not resolved, no contributor replies | Same finding still applies | **Do nothing.** Leave comment as-is. **Never repost.** |
| present | present | **resolved by a core maintainer** | **Maintainer adjudicated.** The maintainer has decided the finding does not need to be fixed. | **Do nothing.** Leave the thread resolved; no reply, no un-resolve, no repost — on this and every later run. Add the key to `resolved_keys` (it stays there for the life of the PR). |
| present | present | **resolved by this lens on an earlier run** (fixed then) | **Reintroduced** (`re-review-state` §6a). The same key is a new occurrence. | **New comment** per §9a with the §6a prefix; count it again in the tag column and `newly added`. The prior thread stays resolved; the ledger is untouched. |
| present | present | **resolved by anyone else** | **Improperly resolved.** Finding still applies on the new head. | **Un-resolve + reply.** GraphQL `unresolveReviewThread`; reply with the improper-resolution body shape (§9). Remove one entry for the key from `resolved_keys` if present. Append maintainer ping per §4. Increment `improperly resolved` in Since-last-run. |
| present | present | not resolved, but contributor has replied | Possible disagreement | **Reply + escalate** per §11. Increment `disagreements escalated` in Since-last-run. |
| present | absent | not resolved | Cleanly fixed | **Resolve:** GraphQL `resolveReviewThread`, **no reply**. Add the key to `resolved_keys` either way: on success the fix is recorded in `since_last_run.resolved` and listed (with its link) in the summary's *Since last run* block; if the mutation is refused, also append the thread URL to `resolve_failed` — still no reply (§"Resolution mechanism"). |
| present | absent | already resolved | Already settled (resolved by the agent on an earlier run, by a core maintainer, or by the contributor after fixing) | **Do nothing.** No reply, no re-resolve. The key is (or is now added) in `resolved_keys`. |
| present (rollup site) | absent | rollup thread still carries a site whose key is present | Partially fixed rollup | **Do not resolve the thread.** Add the site's finding-key to `resolved_keys`. Rollup keys are grouped by thread before this phase; the thread is resolved (rows above) only on the run its last remaining site disappears, and every site key resolved on that run is added to the ledger individually — the thread itself is never a unit of `R`. |
| present (concur) | present | shared thread not resolved | Concurred finding still applies | **Do nothing.** No re-count, no reply; the owning lens handles the thread. |
| present (concur, `adjudicated: true`) | any | any | Already adjudicated on an earlier run | **Do nothing**, anywhere: no re-count, no ledger entry, no reintroduction, keep the entry. |
| present (concur) | present | shared thread **resolved by a core maintainer** | Adjudicated for every concurring lens too | **Do nothing** on the thread. Add the key to `resolved_keys` once and mark the `concur` entry `adjudicated: true` (keep it). |
| present (concur) | present | shared thread **resolved by anyone else** | Improperly resolved from this lens's point of view (§6a: a shared thread is resolved only when every concurring finding is satisfied) | **Un-resolve + reply** exactly as for an own thread (improper-resolution shape, §9), unless the thread already carries an `improper-resolution` reply newer than the resolution — then un-resolve only. Key stays out of `resolved_keys`; increment `improperly resolved`. Contributor disagreement on a shared thread is the owning lens's to escalate. |
| present (concur, not adjudicated) | absent | any | Concurred finding no longer applies | Add its key to `resolved_keys` and **drop the `concur` entry**; the owning lens resolves the shared thread. Nothing to post. |
| present only in `resolved_keys` (a settled note, concurrence or rollup site; no own thread, no `concur` entry) | present | n/a | Reintroduced finding (`re-review-state` §6a) | Treat it as a new occurrence: route per §9a, count it again in the tag column and `newly added`, prefix the body per §6a. **Leave the ledger alone** — the earlier entry settles the earlier occurrence; this one enters the ledger when it is fixed. |
| present (note or rollup site) whose key is in the summary's `promoted` line | present | n/a | Aggregator promoted it to `must fix` (§14) | **Post it inline** as a must-fix comment (§9) with the same finding-key, remove it from `notes` (a rollup site stays in the rollup thread as well), and do **not** increment any tag column — it was counted when first recorded. |
| absent | present, same `(file, symbol)` as a prior but different `finding_class` | n/a | Author attempted a fix that left a different problem in the same spot | **Incorrect-fix follow-up:** new inline comment, body starts with `[<review_label>] **<tag>** Follow-up to <link to prior>: <new issue>`. |
| absent | present, no related prior, **another agent's open thread shares the site-key and describes the same issue** | n/a | Cross-agent duplicate (§6a) | **Record concurrence** in the state block's `concur` list; reply only if own severity is stricter (§6a / §9); count the finding in own state; do not open a new thread. |
| absent | present, no related prior | n/a | Brand-new finding (new code) | **Route it** per §9a: inline thread, per-file rollup, or summary note. Post per §9 / §10. |
| present (note) | absent | n/a | A note-channel finding no longer applies | Drop it from `notes` and add its key to `resolved_keys` (§2); that is what keeps it counted toward `R` on every later run. No reply anywhere. |

### Phase D — Report per-lens state

Compose the refreshed state block (§2) — `reviewed_head` (the new
head), cumulative tag counts, `outstanding`, `run`, `since_last_run`,
verdict, `concur`, `notes`, `resolved_keys`, `resolve_failed` — and return it in the
session completion report for the aggregator to store in the summary
review. Never post it as a review of its own. The
Since-last-run state carries six counters:

- `X resolved` — own threads that became resolved since the prior
  run, however they got there (agent, core maintainer, or contributor
  after fixing), plus notes that no longer apply: `max(0, R − R_prev)`
  per the recomputation below. The lens also lists the thread URLs
  it resolved this run in `since_last_run.resolved_threads` so the
  summary can link them.
- `Y still open` — prior findings that still apply (unchanged threads).
- `Z newly added` — brand-new findings posted this run.
- `W incorrect-fix follow-ups` — same-spot-different-finding-class new
  comments posted this run.
- `V improperly resolved` — threads the agent had to un-resolve this
  run because the finding still applied.
- `U disagreements escalated` — threads on which contributor pushback
  triggered a maintainer ping this run.

### Cumulative columns under re-review

- Tag columns increment when new findings appear.
- Tag columns NEVER decrement on resolution. (Priority 1 guarantee.)
- `outstanding` is **recomputed from thread state every run, never
  carried forward incrementally**: `outstanding = (cumulative tag-column
  sum) − R`, where `R = |resolved_keys|` after this run's Phase C
  actions. The ledger is the single unit of account and counts
  occurrences, as the tag columns do: an own thread that is resolved
  (by anyone) or sits in `resolve_failed`, a rollup site, a note or a
  concurrence whose key disappeared, a concurrence adjudicated — each
  is one entry, appended when it settles and carried forward. Only an
  improper un-resolve (Phase C) removes an entry. A reintroduced key
  adds one to the tag column and nothing to the ledger, so
  `outstanding` rises by exactly one and returns to its prior value
  when the new occurrence is fixed. Threads are never counted
  directly: a rollup thread with three sites contributes zero to `R`
  and its three keys contribute one each as they settle. `R_prev` = prior cumulative sum − prior
  `outstanding`. A finding therefore counts as resolved exactly once,
  however many runs it stays resolved and whichever channel carried
  it.

### Resolution mechanism

A fixed finding is acknowledged by **resolving its thread**, using the
`TOKEN` the external trigger provides (GitHub requires the PR author
or repository **Write** access for `resolveReviewThread`; triage is
not enough):

- GraphQL `resolveReviewThread` collapses the thread in the PR UI.
- **No reply is posted.** The audit trail is the summary's *Since last
  run* block, which links every thread resolved this run. A "Fixed in
  <sha>" reply on an open thread was the single largest source of
  visible clutter (275 such threads were left open across 88 PRs)
  and, once the resolve lands, it says nothing the collapsed thread
  does not.

If `resolveReviewThread` is refused, the lens **does not fall back to
a reply**. It appends the thread URL to `resolve_failed` in its state
block; the aggregator renders every such thread in a visible
`⚠️ Could not resolve N fixed thread(s) — token lacks Write` line at
the top of the summary, so the permissions defect is seen and fixed
rather than papered over. The finding still counts toward `R` (its key is in `resolved_keys`), and the
next run retries the mutation once (`re-review-state` §3b).

Legacy threads that already carry an own `Fixed in` reply from an
earlier run are retried the same way, never replied to again.

### Guardrails (never)

- **Never repost** a finding whose `finding-key` matches an existing
  comment from the same agent on this PR.
- **Never resolve** a comment whose `finding-key` is still present on
  the new head, even if the author replied "fixed".
- **Never silently accept** a non-maintainer's resolution of a thread
  whose `finding-key` is still present. Un-resolve it and reply per
  the improperly-resolved row of phase C.
- **Never reopen, reply to, or repost** a thread a core maintainer
  resolved; that is the adjudication §0 defers to.
- **Never argue.** On disagreement, the agent posts one polite
  escalation reply + maintainer ping, then stops. Further back-and-
  forth is for the maintainer (§11).
- **Never decrement** a tag column. Resolution decrements only
  `outstanding`.
- **Never open a new thread** for a finding whose site-key matches
  another agent's open thread describing the same issue — concur on
  that thread instead (§6a).
- **Never un-resolve** a thread the aggregator recorded as a
  duplicate (summary `duplicates` state, or a legacy
  `reply-kind: duplicate-close` reply); the canonical thread is the
  live home of the finding.
- **Never post a bookkeeping reply** — no "Fixed", no plain "Concur",
  no "Duplicate". The only replies a reviewer posts are the
  improper-resolution, disagreement, incorrect-fix follow-up and
  severity-concurrence shapes of §9; everything else lives in state
  and in the summary.

---

## 8. Agent priorities, in order

Tiebreakers when other contract rules underdetermine behavior. Apply
in strict order; the earlier wins.

**Priority 1 — Do not discard or omit findings.**
- If the agent saw something in-scope, it records it — as an inline
  thread, as a site in a per-file rollup, or as a summary note (§9a).
  Tag conveys importance; the agent does not gatekeep on "is it worth
  saying?", it only chooses the **channel**. Choosing the note channel
  is not omission: the finding is counted, listed in the summary, and
  tracked by finding-key like any other. Scope is defined by the PR
  diff (`pr-diff-scoping`) and, on re-runs, by §7 Phase B (including
  the zero-commit guard); a finding outside that scope is not
  "omitted".
- Low confidence is not a reason to omit (§4).
- Every currently-true finding is reflected in the agent's summary
  counts even if its comment was inherited from a prior run.
- Tag columns are cumulative because of this priority.

**Priority 2 — Prefer suggestions over plain comments.**
- Whenever the agent can express a concrete one-or-few-line diff,
  attach a fenced ` ```suggestion ` block so the reviewer one-click
  applies.
- A best-effort suggestion the agent isn't 100% sure of is still
  preferred over no suggestion at all — add `(best-effort fix; verify
  before applying)` and tag the maintainer per §4.
- Priority 2 never overrides Priority 1.

**Priority 3 — Be succinct, in fixed fields.**
- One finding per inline comment (a rollup is one *class* of finding
  per comment, §9a).
- Comment prose uses the fixed fields of §9 — a one-line title, one
  `Why:` line, one `Fix:` line — each a complete sentence. Fixed
  fields are what keep brevity from "running together": the reader
  always knows which sentence is the consequence and which is the
  remedy. The suggestion block does not count toward the budget —
  GitHub renders it specially and it is the *useful* part of the
  comment.
- No restating context the reviewer sees in the diff; no preamble,
  no restatement of the tag's meaning, no closing pleasantry.
- Summary tables are tables; no narrative around them.
- Must-fix bullets in the summary are one line each.
- The summary's always-visible part (verdict line + must-fix list)
  fits in one screen; everything else is collapsed.
- Priority 3 never overrides Priorities 1 or 2.

---

## 9. Inline comment body shape

Fixed fields, always in the same order, so a reader can find the
consequence and the remedy without reading the whole comment. Graders
comparing this shape against free prose rated it clearer (4.2 vs 3.8)
and far more succinct (4.6 vs 2.4), with zero "runs together"
complaints — the failure mode of earlier brevity attempts.

### Fresh finding (initial post on a thread)

```
[<review_label>] **<tag>** <title: one line, what is wrong, ≤ 12 words>
Why: <one complete sentence — the consequence, or the claim that is now false>
Fix: <one complete sentence — the remedy; or `below` when a suggestion block follows>
Sites: <optional, rollups only — see §9a>

```suggestion
<concrete fix>          (omitted if no fix expressible)
```

(low-confidence-only)
cc @<maintainer1> @<maintainer2> — low-confidence finding, please confirm.

<!-- fprime-agent: <name>; finding-key: <key>; site-key: <skey>; v2 -->
```

`<review_label>` is the agent's `review_label` from
`agent-registry.yml` (e.g., `Security`, `Supply Chain`,
`C++ Design`, `Documentation`, `Design`, `Test Quality`,
`Correctness`, `Operational`, `Maintainability`).

Field rules:

- **Title** names the defect, not the rule ("`len` unchecked before
  `memcpy`", not "possible buffer issue"). No trailing period.
- **Why** is one sentence and states the *consequence* — what goes
  wrong, for whom, under what input — or, for documentation findings,
  the sentence that is now false. Not the rule number, not "this is
  bad practice".
- **Fix** is one sentence naming the remedy. When a suggestion block
  follows, write `Fix: below` (or `Fix: below; <one clause>` when the
  block needs a caveat, e.g. `Fix: below; verify the enum default`).
  A best-effort suggestion still carries `(best-effort fix; verify
  before applying)` per Priority 2.
- **Evidence** the reader cannot see in the diff — the call site that
  reaches the assert, the doc sentence that is now false — goes in the
  `Why:` sentence as a parenthetical or a path:line reference, never
  as a fourth paragraph. If one sentence genuinely cannot carry it,
  add **one** further line beginning `Evidence:`. That is the whole
  budget: title, Why, Fix, optional Evidence/Sites.
- No greeting, no restatement of the tag, no closing remark.
  Priority 3 governs.

### Rollup (several sites, one class, one file — §9a)

```
[<review_label>] **<tag>** <title, phrased for the class> (<N> sites in this file)
Why: <one sentence, the consequence shared by every site>
Fix: <one sentence, the remedy that applies to every site>
Sites: L<n1> `<symbol>`, L<n2> `<symbol>`, L<n3> `<symbol>`[, …]

<!-- fprime-agent: <name>; finding-key: <key1>; site-key: <skey1>; v2 -->
<!-- fprime-agent: <name>; finding-key: <key2>; site-key: <skey2>; v2 -->
<!-- fprime-agent: <name>; finding-key: <key3>; site-key: <skey3>; v2 -->
```

The rollup is anchored at the first site. One footer per site keeps
every site individually trackable on re-review (§7 Phase A). A rollup
carries no suggestion block — a per-site fix is what makes a finding
inline-worthy in the first place (§9a).

### Improper-resolution reply

Posted by the agent on a thread it un-resolved (see §7, phase C).

```
[<review_label>] **Improperly resolved.** This finding still applies on <sha>.

<≤ 3 prose lines: what the agent re-checked and why the finding
remains live; link to the prior comment if helpful>

cc @<maintainer1> @<maintainer2> — contributor resolved without addressing; please adjudicate.

<!-- fprime-agent: <name>; finding-key: <key>; v1; reply-kind: improper-resolution -->
```

### Disagreement escalation reply

Posted by the agent on a thread where the contributor has pushed back
and the finding still applies (see §11).

```
[<review_label>] **Disagreement — escalating.** I still flag this on <sha>; the contributor's response above indicates we disagree.

<≤ 3 prose lines: what the agent re-checked; one sentence
acknowledging the contributor's point and one sentence on why the
agent still flags the finding>

cc @<maintainer1> @<maintainer2> — needs human adjudication.

<!-- fprime-agent: <name>; finding-key: <key>; v1; reply-kind: disagreement -->
```

The `reply-kind` HTML attribute lets the agent recognize its own prior
escalation replies on subsequent runs and avoid double-escalating the
same thread.

### Severity-concurrence reply (cross-agent de-duplication, §6a)

Posted by a reviewer on another agent's open thread **only** when the
reviewer's severity for the same issue is stricter than the thread's
tag. Ordinary agreement is recorded in the lens's `concur` state and
rendered by the summary; it is never replied.

```
[<review_label>] **<stricter tag>** from my scope — <one sentence: the consequence that raises the severity>.

<!-- fprime-agent: <name>; finding-key: <key>; site-key: <skey>; v2; reply-kind: concurrence -->
```

One such reply per agent per thread; the `reply-kind: concurrence`
attribute is the de-dup key on later runs, and legacy plain
concurrence replies carrying it are recognised the same way.

### Severity-promotion reply (severity reconciliation, §14)

Posted by the aggregator on a thread whose finding it promoted to
`**must fix**` on the strength of the finding's own rationale.

```
[Summary] **Promoted to must fix** — <the §1a consequence the rationale demonstrates, ≤ 1 line>.

The summary counts this finding as `must fix`; the original tag above stands as posted.

<!-- fprime-review-summary; site-key: <skey>; v2; reply-kind: severity-promotion -->
```

One severity-promotion reply per thread, ever; the `reply-kind`
attribute is the de-dup key across runs. Reviewers do not treat this
reply as contributor pushback (§11) and never un-resolve or re-tag
because of it.

---

## 9a. Posting channel — inline thread, rollup, or note

Every in-scope finding is recorded (Priority 1); this section decides
**where**. Historically 71 % of findings were `could fix` or
`suggestion`, each opened its own thread, and the maintainer had to
scroll past all of them to find the blockers. Three channels:

| Channel | What goes there | Rendered as |
|---|---|---|
| **Inline thread** | Every `must fix`. Any finding that carries a concrete ` ```suggestion ` block. Any below-must-fix finding whose *understanding* needs the code in view: behaviour changes, state-machine / sequence, ownership / lifetime, timing / resource, safety, or any judgment call the author may reasonably dispute. | One comment per finding (§9). |
| **Rollup** | Three or more below-must-fix findings of the **same `finding_class` in the same file** that share one remedy (e.g. five missing `const`, four unqualified `sizeof`). | One comment at the first site, `Sites:` line, one footer per site (§9 Rollup). |
| **Note** | A below-must-fix finding that is self-explanatory from its title alone, has no suggestion block, and needs no code in view to act on (a typo, a naming nit, a stale comment, a missing `explicit`). | One line in the summary's collapsed **Notes** section, attributed to the lens; carried in the lens's `notes` state (§2). |

Decision order: must-fix → inline. Suggestion block present → inline.
Judgment/behaviour class → inline. ≥ 3 same-class same-file → rollup.
Otherwise → note. When genuinely unsure whether the author needs the
code in view, post inline: a misplaced inline comment costs one
thread; a misplaced note costs the author a search.

What the channels do **not** change:

- Every finding is counted in the lens's tag columns and
  `outstanding`, whatever its channel.
- A note has a finding-key and a site-key and is re-checked on every
  run exactly like a thread; when the key disappears, it is dropped
  from `notes` and its key entered in `resolved_keys`, which is how
  it stays counted as resolved on every later run (§7 Phase C).
- A rollup's sites are individually keyed. Phase C groups a rollup's
  keys by thread: a site that disappears goes into `resolved_keys`
  and the thread stays open; the thread is resolved only on the run
  its last site disappears. The summary lists the remaining count.
- Severity reconciliation (§14) may promote a note or rollup site to
  `must fix`; the aggregator then lists it in the must-fix list with
  its summary line as the link target, records the finding-key in
  the summary's hidden `promoted` line, and the **next** run of the
  owning lens posts it inline (§7 Phase C, promoted row) under the
  same key without counting it again.
- A note is never a way to avoid saying something awkward. If the
  title needs a `Why:` to be understood, it is not a note.

---

## 10. Posting mechanics

Inline review comments are posted through the GitHub Pull Request
Review API. Mechanics, the suggestion-block syntax, the GraphQL
mutations (`resolveReviewThread`, `unresolveReviewThread`), and the
`TOKEN` env var live in `.github/skills/post-inline-review/SKILL.md`.

Each reviewer submits **at most one** PR review per run (event:
`COMMENT`, **empty body**) whose inline comments are its new findings
— on run 1 and on every later run alike. A run with no new inline
comment posts no review at all. Per-lens state is never posted by the
reviewer; it is reported to the orchestrator and stored in the summary
review's `lens-state` line (§2).

The aggregator submits a single PR review keyed by its HTML marker
(`<!-- fprime-review-summary v1 -->`). The review event is:

- **`APPROVE`** when both CI safety and Merge readiness are `Go`.
- **`REQUEST_CHANGES`** when either verdict is `No-Go`.

The review body is the blocker-first summary (see
`review-summary.agent.md`) and carries, immediately after the marker,
the `<!-- reviewed_head: <sha> -->` line and the hidden
`<!-- lens-state: [...] -->` and `<!-- duplicates: [...] -->` lines
that hold every lens's state (§2). On re-runs the aggregator updates
the body in place when the event is unchanged, and
dismisses-and-resubmits only when the event flips or the prior review
is already `DISMISSED` (§6). The hidden state travels with the body
either way, so an edit or a resubmit never loses it.

On an `APPROVE` event the aggregator additionally requests the core
maintainers (`maintainer-lookup` §1b) as reviewers, once per PR,
recorded in its `<!-- maintainers_requested: -->` line
(`review-summary.agent.md` §5i). No other agent requests reviewers.

---

## 11. Disagreement handling

When the contributor pushes back on a comment via replies (rather than
resolving the thread), the agent does not argue or re-litigate. It
elevates to a code owner once and stops.

### Detection

During phase C (§7), a thread qualifies for disagreement escalation
iff ALL of the following hold:

1. The prior `finding-key` is still present on the new head (the
   finding still applies in the agent's view).
2. The thread is **not** resolved (resolved threads use the
   improperly-resolved row of phase C instead).
3. The thread has at least one reply from a user other than the
   agent itself.
4. The agent has **not** already posted a `reply-kind: disagreement`
   reply on this thread in a prior run (the HTML attribute on the
   agent's own replies is the de-dup key — escalate once per thread).

Low-confidence findings (§4) are still eligible: the maintainer ping
is the whole point.

### Action

1. **Reply once** on the thread using the disagreement-escalation
   reply shape (§9). One reply, no further back-and-forth.
2. **Tag the code owner / maintainer** via
   `.github/skills/maintainer-lookup/SKILL.md` (same 4-step lookup as §4).
3. **Do not resolve the thread.** Leave it open for the maintainer.
4. **Increment `disagreements escalated`** in the per-agent review's
   `since_last_run` metadata (§7 phase D).

### Relation to low-confidence pings (§4)

- Low-confidence ping = on the **initial** comment, because the
  agent isn't fully sure.
- Disagreement ping = on a **reply**, because the contributor and
  the agent disagree and a human needs to adjudicate.

Both use the same maintainer-lookup skill; the trigger and posting
site differ. A given thread can in principle carry both
(low-confidence on the first post; disagreement on a later reply) —
each ping happens at most once per thread.

---

## 12. Roles

Each entry in `agent-registry.yml` carries a `role` field:

- `orchestrator` — the single human entry point. Drives the reviewer
  sessions and then performs the aggregation itself. Posts no inline
  comments.
- `reviewer` — posts inline comments only (no visible summary table,
  no metadata review). Submits at most one empty-body PR review
  (event: `COMMENT`) per run carrying its new inline comments, routes
  minor findings to the summary's Notes (§9a), and reports its state
  block (§2) to the orchestrator.
- `aggregator` — consumes per-lens state blocks and inline comments,
  reconciles severity (§14), then submits ONE PR review with event
  `APPROVE` or `REQUEST_CHANGES` based on the consolidated Go/No-Go
  verdict; the review body carries every lens's state.

The orchestrator iterates over `role: reviewer` entries to drive
reviewers, then executes the `role: aggregator` entry.

**A role is a lens, not a process.** How many sessions the review
occupies is an orchestration decision (§13) and never changes what is
posted:

- Several `role: reviewer` lenses may share one review session. Each
  lens still posts its own inline comments under its own
  `review_label`, reports its own state block (§2), and keeps its own
  run ordinal. A reader of the PR cannot tell how lenses were packed.
- The orchestrator executes the `role: aggregator` entry itself rather
  than delegating it to a further session; the aggregator's own file
  governs the summary's content, and while acting in that role the
  orchestrator is bound by every aggregator rule, including the
  prohibition on analyzing code or opening new threads. The aggregator
  remains separately invocable for debugging.
- Lenses never merge, share, or trade findings inside a shared
  session. Each lens applies its own agent file and reaches its own
  conclusions; Priority 1 (§8) binds each lens individually. Sharing a
  session saves startup cost, not review work.

---

## 13. Review effort budget

Review cost is dominated by session startup and by deep verification —
full-file reads, caller tracing, reachability arguments. This section
bounds where that effort is spent. It bounds **investigation only**;
§1a governs tagging, and Priority 1 (§8) still forbids discarding
anything the agent actually found.

### 13a. Session packing

The orchestrator packs the reviewer set into one session per
`review_group` in `agent-registry.yml`, at most four lenses per
session, and runs the groups in the fixed order that registry's header
specifies. A reviewer with a missing or unrecognized `review_group`
runs in its own session — never folded in silently, never skipped.

### 13b. Must-fix-first budget (non-exempt lenses)

A missed `must fix` is an unrecoverable failure; a missed `could fix`
or nit-tier `suggestion` is an acceptable saving. So each non-exempt
lens sweeps its scope for candidate sites, ranks them by the worst tag
they could plausibly carry, and spends deep verification on the
candidates that could be `must fix` — plus any below-must-fix
candidate cheap to confirm from context already read.

- The must-fix search itself is never weakened: full-file reads and
  caller tracing remain mandatory before asserting **or** dismissing a
  must-fix (§CONTEXT MANDATE in the orchestrator's kickoff prompts).
- Do not open new files or trace new call chains solely to firm up a
  `could fix` or `future work` item. Report those when already
  evident, and stop after roughly three per lens.
- Note in the lens's state block how many below-must-fix candidates
  were left unexplored: `"unexplored_below_must_fix": N`.

### 13c. Slim first-pass reading (non-exempt lenses, run 1 only)

On run 1 a non-exempt lens reads of this contract only §0, §1/§1a,
§3, §4, §8, §9 and §9a — the sections that govern what it is looking
for, how to tag it, how to word it, and where to put it. The remaining sections govern posting mechanics,
de-duplication and re-review state, which the lens follows through
`post-inline-review` and `re-review-state` as it posts.

On **run ≥ 2** this narrowing does not apply: re-review needs §6, §6a,
§7 and §11 in full, and a lens that skipped them would repost,
mis-resolve, or re-escalate. Each lens reads its own agent file in
full, always; of the skills that file references, it reads the ones
whose subject matter the diff actually touches.

### 13d. Safety exemption (mandatory)

A lens with `contributes_to_ci_safety: true` — the `safety` group — is
**exempt from §13b and §13c**. It reads in full, caps nothing, and for
every value this PR lets ground or hardware touch (commands,
parameters, uplinked file content, sequence directives, config
constants a deployment can change) traces that value from its entry
point to every `FW_ASSERT`, buffer-size computation, index, and array
write it can reach, across files and components, per
`.github/skills/fprime-ground-input-tracing/SKILL.md` and
`.github/skills/fprime-hardware-input-tracing/SKILL.md`. The rationale
names which entry point reaches which assert.

A reachable assert or bound violation driven by a ground-settable
value is a `must fix`. Cost is never a reason to stop that trace
early: uniformly budgeting every lens measurably lost exactly this
finding class, which is why the exemption exists. The saving comes
from the other groups.

---

## 14. Severity reconciliation at aggregation

Severity tagging is a cheap centralized judgement and an expensive
distributed one: reviewers reliably find the defective site, then
disagree about which tag it carries — a disagreement two runs of the
same reviewer set exhibit against each other. The aggregator therefore
arbitrates severity once, holding every finding and its rationale.

Before composing the summary, the aggregator re-reads each outstanding
finding's rationale against §1/§1a and **promotes** any finding whose
stated consequence is must-fix-tier though its reviewer tagged it
lower. The promotion list is exactly §1a's consequence list.

- **Never demote** a reviewer's `**must fix**`. Arbitration is
  one-directional; a maintainer resolving the thread is how a
  disputed must-fix is settled (§0).
- Promote on the finding's own stated rationale, not on the
  aggregator's fresh analysis of the code. The aggregator does not
  analyze code (`review-summary.agent.md` §Role); if the rationale
  does not demonstrate the consequence, the tag stands.
- A promoted finding counts as `must fix` in the summary's Totals,
  `Outstanding must-fix items`, and both verdicts (§5c) — so a
  promotion can flip `Merge readiness` to `No-Go`.
- Reviewer hidden-metadata counts are **not** rewritten (as in §5h):
  each lens owns its own counts, and the aggregator adjusts only its
  own consolidated rendering.
- Every promotion, and every deliberate non-promotion of a finding the
  aggregator considered, is recorded in the summary's promotion log
  (`review-summary.agent.md` §5j) so the arbitration is auditable
  rather than silent.
- The aggregator notes the promoted tag on the thread with one
  `reply-kind: severity-promotion` reply (§9), once per thread ever,
  so the maintainer reading the thread sees the tag the summary used.
