# CLAUDE.md — F´ guidance for Claude Code

The canonical guidance for AI coding agents in this repository — including
Claude Code — is the repo-root [`AGENTS.md`](AGENTS.md). Read it first.

The skills you may need to load on top of `AGENTS.md`:

- [`.agents/code-review.md`](.agents/code-review.md) — reviewing C++ changes
- [`.agents/component-author.md`](.agents/component-author.md) — new components
- [`.agents/unit-test-author.md`](.agents/unit-test-author.md) — unit tests

Other policy files Claude must honor:

- [`AI_POLICY.md`](AI_POLICY.md) — AI usage disclosure
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — contribution flow & CCB
- [`.github/untrusted-pr-review-policy.md`](.github/untrusted-pr-review-policy.md) — security posture for PRs

This file is intentionally a thin pointer so `AGENTS.md` remains the single
source of truth for every AI tool. See `AGENTS.md` §5 for the full skill
catalog and §8 for hard constraints.
