# F Prime Copilot Instructions

The canonical guidance for AI coding agents in this repository — including
GitHub Copilot (chat, code review, and the custom agents in
[`agents/`](agents/)) — is at [`../AGENTS.md`](../AGENTS.md). Read it before
doing anything else in this workspace.

The canonical reviewer behavior, the 32 F Prime review rules, and the
required review output format live in
[`../.agents/code-review.md`](../.agents/code-review.md). The
[`agents/`](agents/) wrappers (code review, component author, unit-test
author) load those canonical skills. The multi-agent PR review system
(orchestrator, security, supply-chain, summary) is documented in
[`../AGENTS.md`](../AGENTS.md) and lives in
[`agents/`](agents/) alongside its shared contract and skills under
[`agents/_shared/`](agents/_shared/).

This file is intentionally a pointer with no inlined instructions, so
[`../AGENTS.md`](../AGENTS.md) and [`../.agents/`](../.agents/) remain the
single source of truth for every AI tool.
