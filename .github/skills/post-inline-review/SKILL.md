---
name: post-inline-review
description: Use when posting inline review comments, the per-agent summary review, or interacting with GitHub review threads (resolve, un-resolve, reply) for an F Prime PR review agent.
---

# Skill: Post an inline review on a GitHub PR

Reusable procedure for any F Prime review agent that needs to post
inline review comments, the per-agent summary review, or interact with
GitHub review threads (resolve / un-resolve / reply).

This skill assumes:

- An appropriately-permissioned token is exposed as `${TOKEN}` in the
  environment (provided by whatever external trigger invoked the
  agent). Required scopes: read access to the repository, write
  access to pull-request reviews, and write access to discussions /
  review threads for the GraphQL mutations.
- The agent knows the owner, repo, PR number, head commit SHA, and
  its own short name (matches the `agent-registry.yml` entry).

---

## 1. Suggestion-block syntax

GitHub renders a fenced block whose info string is exactly
`suggestion` as a one-click "Apply suggestion" diff against the line
range the comment is anchored to.

```markdown
[Security] **must fix** Unbounded copy from ground argument.

Validate `len` against the destination buffer before copying. The
incoming `len` is ground-controlled and can exceed `sizeof(dst)`.

```suggestion
if (len > sizeof(dst)) { return Status::INVALID_LENGTH; }
memcpy(dst, src, len);
```

<!-- fprime-agent: security-review; finding-key: abc123…; v1 -->
```

The suggestion block replaces the entire line range the comment is
anchored to. For multi-line replacements, anchor the comment to the
full range (`start_line` + `line`) rather than a single line.

---

## 2. Posting one review with many inline comments

A single PR review can carry many inline comments. Prefer one review
per agent run rather than many small reviews — the GitHub UI groups
them together.

Every run — the `body` is **empty**; the review exists only to carry
the inline `comments[]`, and it is posted only when there is at least
one new inline comment. Per-lens state is never posted (see §4):

```http
POST /repos/{owner}/{repo}/pulls/{pull_number}/reviews
Authorization: Bearer ${TOKEN}
Accept: application/vnd.github+json
Content-Type: application/json

{
  "commit_id": "<head SHA>",
  "event": "COMMENT",
  "body": "",
  "comments": [
    {
      "path": "Svc/CmdDispatcher/CmdDispatcher.cpp",
      "line": 142,
      "side": "RIGHT",
      "body": "[Security] **must fix** `len` unchecked before `memcpy` from ground argument\nWhy: A command with `len > sizeof(buf)` overruns `buf` and corrupts the dispatcher state.\nFix: Reject `len > sizeof(buf)` with a command-response error before the copy.\n\n<!-- fprime-agent: security-review; finding-key: abc; site-key: s1; v2 -->"
    },
    {
      "path": "Svc/CmdDispatcher/CmdDispatcher.cpp",
      "start_line": 200,
      "line": 207,
      "start_side": "RIGHT",
      "side": "RIGHT",
      "body": "[Security] **suggestion** Return value of `read()` discarded\nWhy: A short read leaves the tail of `buf` uninitialised and it is later sent to the ground.\nFix: below\n\n```suggestion\n…\n```\n\n<!-- fprime-agent: security-review; finding-key: def; site-key: s2; v2 -->"
    }
  ]
}
```

A **rollup** comment (review contract §9a) is one entry in
`comments[]` anchored at the first site, with a `Sites:` line and one
footer per site:

```json
{
  "path": "Svc/CmdDispatcher/CmdDispatcher.cpp",
  "line": 88,
  "side": "RIGHT",
  "body": "[C++ Design] **could fix** `sizeof` on a pointer parameter (4 sites in this file)\nWhy: Each call copies `sizeof(void*)` bytes instead of the buffer length, truncating the payload.\nFix: Use the length parameter that accompanies each buffer.\nSites: L88 `dispatch`, L131 `enqueue`, L164 `flush`, L202 `reset`\n\n<!-- fprime-agent: fprime-code-review; finding-key: k1; site-key: s1; v2 -->\n<!-- fprime-agent: fprime-code-review; finding-key: k2; site-key: s2; v2 -->\n<!-- fprime-agent: fprime-code-review; finding-key: k3; site-key: s3; v2 -->\n<!-- fprime-agent: fprime-code-review; finding-key: k4; site-key: s4; v2 -->"
}
```

`event: COMMENT` is correct for all **reviewer** agents — never
`APPROVE` and never `REQUEST_CHANGES` (the merge-readiness verdict
is the aggregator's job, not the individual reviewer's).

The **aggregator** (`review-summary`) uses `APPROVE` or
`REQUEST_CHANGES` based on its CI safety and merge readiness
verdicts — see review-contract.md §10.

`commit_id` MUST be the head SHA the agent analyzed; this is what
binds the comments to specific line positions. It is **not** a
record of the last reviewed head — the `reviewed_head` field of the
lens's state block is (review contract §2).

---

## 3. Replying to an existing review thread

Replies post to the in-line comment that started the thread (the
`in_reply_to` field).

```http
POST /repos/{owner}/{repo}/pulls/{pull_number}/comments/{comment_id}/replies
Authorization: Bearer ${TOKEN}
Content-Type: application/json

{
  "body": "[Security] **Improperly resolved.** The finding is still present at <head-sha>: <one sentence>.\n\ncc @<maintainer> — please adjudicate.\n\n<!-- fprime-agent: security-review; finding-key: <key>; site-key: <skey>; v2; reply-kind: improper-resolution -->"
}
```

Replies are the **exception**, not the bookkeeping channel. They are
used only for:

- The **Improperly resolved.** reply on an un-resolved thread (see
  the improper-resolution body shape in the review contract §9).
- The **Disagreement — escalating.** reply when contributor pushback
  meets the escalation criteria (review contract §11).
- The **severity-concurrence** reply a reviewer posts on another
  agent's thread when its own severity for the same issue is
  **stricter** than the thread's tag (review contract §6a / §9,
  `reply-kind: concurrence`). Ordinary agreement is recorded in the
  lens's `concur` state and never replied.
- The **severity-promotion** reply the aggregator posts when it
  promotes a finding to must-fix (review-summary.agent.md §5j).

Never posted, by anyone:

- `Fixed in <sha>.` — a clean fix is a silent `resolveReviewThread`
  (§5); the summary's *Since last run* block links every thread
  resolved. A refused resolve goes in the lens's `resolve_failed`
  state and is rendered visibly by the summary, not replied.
- Plain `Concur` — recorded in `concur` state.
- `Duplicate — consolidated into …` — the aggregator resolves the
  duplicate silently and records the pair in the summary's
  `duplicates` line (review-summary.agent.md §5h).

---

## 4. Per-lens state — reported, never posted

Reviewers post no metadata review and never call
`PUT /repos/{owner}/{repo}/pulls/{pull_number}/reviews/{review_id}`.
Each lens returns its state block (review contract §2) to the
orchestrator in its session completion report, as one
`<!-- lens-state: {...} -->` JSON line; the aggregator writes every
lens's block into the hidden `lens-state` line of the single summary
review, which is the only persistent store of per-lens state on the
PR. This removes the ~10 metadata-only review objects per run that the
previous design left on the PR page and the `404`/`403` in-place-edit
failures that multiplied them.

The **aggregator** is the only agent that edits a review body in
place (its own summary, `review-summary.agent.md` §5d):

```http
PUT /repos/{owner}/{repo}/pulls/{pull_number}/reviews/{review_id}
Authorization: Bearer ${TOKEN}
Content-Type: application/json

{ "body": "<!-- fprime-review-summary v1 -->\n<!-- reviewed_head: <head SHA> -->\n<!-- run: N -->\n<!-- maintainers_requested: ... -->\n<!-- lens-state: [...] -->\n<!-- duplicates: [...] -->\n## Automated review — run N · `<sha7>`\n..." }
```

This endpoint changes only the summary body; the review's state,
`commit_id`, and attached inline comments are unchanged, and no
notification is sent. The aggregator dismisses-and-resubmits only
when the verdict event flips (`APPROVE` ↔ `REQUEST_CHANGES`), and
GitHub permits that because those events are dismissable; a
`COMMENTED` review returns `422 Can not dismiss a commented pull
request review`. If the `PUT` fails with `404`/`403` (the prior
summary was authored under a different token identity), submit a
fresh summary review carrying the full hidden state and let later
runs take the newest marker match.

**Legacy PRs** may still carry `<!-- fprime-agent: <name> v1 -->`
metadata reviews from the previous design. Read them once as the
fallback prior state (`re-review-state` §1b-bis); never edit or
re-post them.

---

## 5. Resolving a review thread (GraphQL)

Two mutations, both keyed by the **thread ID** (NOT the comment ID).
The thread ID is fetched via a GraphQL query against
`pullRequest.reviewThreads` filtered by the comment's databaseId.

### resolveReviewThread

```graphql
mutation Resolve($threadId: ID!) {
  resolveReviewThread(input: { threadId: $threadId }) {
    thread { id isResolved }
  }
}
```

Headers:

```
Authorization: Bearer ${TOKEN}
Accept: application/vnd.github+json
```

### unresolveReviewThread

```graphql
mutation Unresolve($threadId: ID!) {
  unresolveReviewThread(input: { threadId: $threadId }) {
    thread { id isResolved }
  }
}
```

Used by the improperly-resolved row of the re-review decision table
(review contract §7, phase C).

### Fetching thread state

```graphql
query Threads($owner: String!, $name: String!, $number: Int!) {
  repository(owner: $owner, name: $name) {
    pullRequest(number: $number) {
      reviewThreads(first: 100) {
        nodes {
          id
          isResolved
          resolvedBy { login }
          comments(first: 100) {
            nodes {
              id
              databaseId
              author { login }
              body
              createdAt
            }
          }
        }
      }
      # paginate via pageInfo.endCursor / hasNextPage as needed
    }
  }
}
```

The agent uses this to:

1. Index its own prior comments by `finding-key` (parsed out of the
   HTML footer).
2. Read `isResolved` and `resolvedBy.login` (drives the
   maintainer-adjudicated vs. improperly-resolved decision — a
   core maintainer's resolution is final, `re-review-state` §3a-0).
3. Read the reply chain (drives disagreement detection in review
   contract §11).

---

## 6. Failure modes and fallbacks

| Failure | Fallback |
|---|---|
| `resolveReviewThread` returns `403` / `FORBIDDEN` (the token lacks Write on the repository) | **Do not reply.** Append the thread URL to the lens's `resolve_failed` state; the summary renders it visibly as `⚠️ Could not resolve N fixed thread(s) — token lacks Write` so the permission is fixed. The thread counts as resolved in the `re-review-state` §4 recomputation (the finding is gone); the next run retries the mutation once. |
| `unresolveReviewThread` returns `403` | Post the improperly-resolved reply anyway. The thread remains visibly resolved on GitHub but the reply + maintainer ping is visible inline. Increment `improperly resolved` regardless. |
| Inline-comment POST returns `422 Pull Request Review thread cannot be created on this line of the diff` | The line is not in the PR's diff. Re-anchor to the nearest line that is in the diff (typically the function header) and prefix the comment body with `(Anchored above the offending line; the diff does not include line N.)` |
| `PUT .../reviews/{review_id}` (summary body update, aggregator only) returns `404`/`403` | Submit a fresh summary review carrying the full hidden state (§4). Never attempt `/dismissals` on a `COMMENTED` review. |
| Token missing entirely | Fail fast. The agent emits a single line to the orchestrator: `Cannot post review: TOKEN not provided.` and exits. The orchestrator treats this as a FAILED reviewer per review-summary.agent.md §5. |

---

## 7. Rate limits, retries, and pagination

- Treat any `5xx` response as retryable with exponential backoff
  (1s, 2s, 4s, 8s, give up).
- Treat `429` and `403` with `X-RateLimit-Remaining: 0` as backoff
  per `X-RateLimit-Reset` header.
- Treat `403` without a rate-limit header as permission failure (no
  retry).
- Do NOT retry `422` errors — they indicate a malformed request and
  retrying will produce the same error.

### Secondary rate limits — abort, never retry

`TOKEN` is shared with other services, so tripping GitHub's
secondary (abuse-detection) limit for content creation disrupts
more than this review. On a `429`, or a `403` whose body mentions
"secondary rate limit":

- Stop issuing content-creation calls (POST/PATCH to comments,
  reviews, statuses) immediately.
- Report the abort to the orchestrator as
  `FAILED: secondary rate limit`; do not retry or wait it out.
- Keep write bursts small in the first place: space
  content-creation calls out rather than firing them all at once.

### Pagination discipline

- Every list endpoint returns one page (default 30 items). Always
  request `per_page=100` and loop until a short page is returned
  (REST) or `hasNextPage` is false (GraphQL). Silent truncation
  from an unpaginated call drops findings and PRs without any
  error.
- For the Search API, verify the total number of items fetched
  equals `total_count`; on mismatch, log a warning — the search
  index may be inconsistent and results may be missing.
- The Search API has its own 30 req/min limit; pause briefly
  between search pages.

---

## 8. Worked example: the full flow on one PR

1. Read PR head SHA. Bind every subsequent call to this SHA.
2. Take the lens's prior state block from the kickoff prompt (or the
   summary review's `lens-state` line; legacy: its old metadata
   review). Note its run count, `reviewed_head`, `notes`,
   `resolve_failed`, and build the `finding-key` index from its
   prior inline comments plus its notes.
3. Run the agent's analysis on the new head. Compute the new
   `finding-key` set; scope new below-must-fix findings to the diff
   since `reviewed_head` (`re-review-state` §2a) — none at all on a
   zero-commit re-run.
4. Match prior vs current per review contract §7 phase C. Build the
   action list: `post-new` (inline / rollup / note),
   `resolve-thread`, `reply-improper`, `unresolve-thread`,
   `reply-disagreement`, `record-concurrence`,
   `post-incorrect-fix-followup`, `do-nothing`.
5. Execute the action list. Resolves are silent; refused resolves go
   in `resolve_failed`.
6. Only if there are new inline comments, POST them as one
   empty-body review (§2).
7. Return `<lens>: completed` plus the refreshed
   `<!-- lens-state: {...} -->` block to the orchestrator (§4).

---

## External references

The endpoints and mutations referenced in this skill are documented
on the GitHub developer site. If a request behaves differently from
what this skill describes, the GitHub documentation is authoritative;
open a PR to update this skill so the next agent sees the corrected
behavior.

- GitHub REST API — Pulls: Reviews:
  https://docs.github.com/en/rest/pulls/reviews
- GitHub REST API — Pulls: Comments (inline review comments):
  https://docs.github.com/en/rest/pulls/comments
- GitHub REST API — Issues: Comments (top-level PR comments via the
  shared issue-comments endpoint):
  https://docs.github.com/en/rest/issues/comments
- GitHub GraphQL — `PullRequestReviewThread` object
  (`resolveReviewThread` / `unresolveReviewThread` mutations):
  https://docs.github.com/en/graphql/reference/objects#pullrequestreviewthread
- GitHub GraphQL — Mutations index:
  https://docs.github.com/en/graphql/reference/mutations
- GitHub REST API — Rate limiting and conditional requests:
  https://docs.github.com/en/rest/overview/resources-in-the-rest-api#rate-limiting

When the API surface evolves (endpoint paths, scope requirements,
response shapes), update the relevant section of this skill in the
same PR that addresses the change so downstream agents inherit the
new behavior automatically.
