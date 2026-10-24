# Networking

Session model, matchmaking, host migration, community moderation, and inventory integrity. AI-specific networking (the Purpose system across peers) is in [AI.md](AI.md) → "Networking". Destruction's server/client split is in [Physics.md](Physics.md).

## Current direction

Player-hosted (listen server) sessions, no dedicated server infrastructure, no traditional anti-cheat product. This shapes everything below.

## Architecture: the big picture

No fully "solved" alternative to the standard tradeoff was found. Server-authoritative integrity and player-hosted availability/cost are in real tension, and the practical industry answer is tiering (dedicated servers for anything with competitive stakes, community/listen servers for casual) rather than a new trustless model. Ideas worth prototyping rather than dismissing:
- **Deterministic lockstep** (classic RTS model) or **GGPO-style rollback netcode**, specifically for AI-heavy regions: replicate inputs/outcomes rather than state. Full bitwise determinism in UE5 (Chaos physics, multithreaded task graph) is hard and is the main risk to derisk early if pursued.
- **Redundant/cross-validated peer authority** (peers vote/checksum-compare) to raise the cost of cheating in P2P. Adds compute/bandwidth cost and only raises the bar; it doesn't remove the trust problem.
- **Client-authoritative regions:** distribute AI simulation *compute* across peers by region, but keep *arbitration* of anything that mutates shared/player state authoritative. Purely cosmetic simulation can be fully desync-tolerant.

## Replication system: Iris

**Direction: build on Iris from the start**, Epic's newer replication system, rather than the generic (legacy) replication system.

**Why it fits this project:**
- Built for larger, more interactive worlds and higher replicated-object counts: replication data is separated from game-thread data so work can run concurrently, and work is shared across objects and connections. Many replicated AI is exactly that workload.
- **Listen-server VR makes it more important, not less.** The host is a player's machine that is also rendering VR. Replication work moved off the game thread is game-thread time the host's VR frame gets back.
- No legacy netcode exists yet, so there's nothing to port. Adopting it later means porting every replicated class.

**Risks:**
- Epic's documentation still labels Iris **Experimental** in 5.7 ("use caution when shipping with it"); some third-party write-ups describe it as Beta. Expect API changes between engine versions.
- Engine systems and plugins vary in Iris support (e.g. GAS, Mass replication). Verify each one the project depends on before relying on it.
- **Mitigation:** Iris is a runtime toggle (`-UseIrisReplication=1` / `=0`). Keep replicated code working on both systems while it's experimental, which also makes A/B profiling trivial.

**Enabling it** (requires converting the project to C++, which is needed anyway for the AI system):
1. `AO.uproject` → `Plugins`: `{ "Name": "Iris", "Enabled": true }`.
2. `AO.Build.cs`: `SetupIrisSupport(Target);`.
3. ~~`bUseIris = true;` in the Target.cs files~~: **not needed in 5.7.** The setting no longer exists in UnrealBuildTool (Iris is always compiled in), so adding it breaks the build. Epic's docs still list it.
4. `DefaultEngine.ini` → `[SystemSettings]`:
   - `net.SubObjects.DefaultUseSubObjectReplicationList=1` (required)
   - `net.Iris.UseIrisReplication=1` (required past 5.1)
   - `net.IsPushModelEnabled=1`. Push model is worth using regardless: properties are marked dirty explicitly instead of compared every frame. Leave `net.Iris.PushModelMode` at its default (2 = enabled, runtime togglable) to keep A/B testing possible.
5. Net driver: the engine's `BaseEngine.ini` already allows Iris on `GameNetDriver` (`+IrisNetDriverConfigs=(NetDriverDefinition=GameNetDriver, bCanUseIris=true)`), so no project entry is needed.
6. Verify at runtime with the `LogIris` log category.

**Status (2026-10-23):** steps 1, 2, 4 applied to `C:\Dev\AO\AO`.

**Profiling:** add a replication test to the VR viability test ([Engine.md](Engine.md)): N replicated AI on a listen-server host, measuring host game thread and bandwidth, Iris vs. generic.

Sources: [Introduction to Iris (Epic docs)](https://dev.epicgames.com/documentation/unreal-engine/introduction-to-iris-in-unreal-engine?lang=en-US), [Iris Replication in Unreal Engine: Should You Opt In? (StraySpark)](https://www.strayspark.studio/blog/iris-replication-unreal-engine-opt-in-2026), [Iris + Push Model in UE 5.7 (StraySpark)](https://www.strayspark.studio/blog/iris-push-model-ue5-7-replication-frameworks), [Iris: 100 Players in One Place (BorMor)](https://bormor.dev/posts/iris-one-hundred-players/), [Iris Networking UE5.7+ with FastArray (Epic forums)](https://forums.unrealengine.com/t/iris-networking-ue5-7-with-fastarray-example/2684404)

## Matchmaking, not a server browser

Use UE5's session/matchmaking APIs (Online Subsystem, or Epic Online Services for a turnkey cross-platform path) to place players into open sessions programmatically instead of showing a browsable list. "Quick match" buttons work this way in most UE multiplayer titles.

**Hiding who is hosting** is the harder part and depends on the session backend:
- The host doesn't experience its own round-trip latency, which is an inherent listen-server tell.
- Any debug UI or log that reveals host status must be stripped from shipping builds.
- Raw platform lobbies (e.g. Steam) expose lobby ownership to every member by design. Fully suppressing that needs a broker/relay layer in front of the platform's session primitive, a meaningfully bigger lift than the built-in session APIs. EOS sessions give more control over what's exposed.

Decide how much of that investment is worth it before committing to a backend.

## Host migration

UE has no turnkey host migration for listen servers; it's built per project. The pieces: detect host loss (graceful leave and ungraceful timeout), elect a replacement, start it listening, reconnect the remaining clients, and restore world state.

- **Phase 1 (build first): graceful reload.** Detect host departure, elect a replacement (simplest viable: lowest ping to the remaining group). That peer starts a fresh listen server from a periodically serialized state snapshot: inventories, objective progress, alive AI + Purpose state (see [AI.md](AI.md)), the item pool. Remaining clients get a brief "Reconnecting…" transition. This is what most player-hosted survival/co-op titles ship; a short visible transition is acceptable UX.
- **Phase 2 (only if the interruption becomes a real problem):** route all client traffic through a relay (EOS's P2P relay fits if EOS is used for matchmaking) so authority can move between peers without any client connection dropping. A much bigger backend investment; don't start here.

## Community moderation (no anti-cheat product)

- **Server-side sanity checks** (max movement speed, weapon fire-rate/damage bounds, etc.): "free" anti-cheat any server-authoritative design should have regardless, and it catches the cheapest cheat tools.
- **Host tools:** persistent bans keyed to platform ID (not IP), spectate-any-player, basic suspicious-activity signals (aim-snap patterns, abnormal accuracy) so decisions have evidence behind them.
- **Optional shared ban lists** across community hosts who opt in. Needs safeguards so a malicious host can't poison a shared list unilaterally (reputation weighting or manual review rather than fully automatic).
- **Careful vote-kick** (cooldowns, minimum session size, required reason, logged) as in-session recourse without the host present; vote-kick is itself a griefing vector if unguarded.
- **Honest expectations** in-product ("community-moderated") rather than implying AAA-grade protection.

## Inventory integrity (item duplication)

**Model:** every item that could ever exist, including what players bring in, gets a unique, permanent ID down to the individual instance (a stack of 5 grenades is 5 IDs, not one ID with a count). Items are never created or destroyed during a session; spawning, picking up and dropping are state transitions (owner/location) on existing records. Records are lightweight structs, not Actors; only the currently relevant subset gets an Actor.

**What actually prevents duplication:** a unique ID is necessary but not sufficient. The ID stays unique even during a dupe bug; the real failure is the authoritative system treating the same ID as existing in two places at once (e.g. two near-simultaneous transactions both read "item X is here" before either commits, a TOCTOU race). The real defense is **atomic, single-writer ownership transfer per ID**: a transaction applies fully once or not at all, and can't be replayed to apply twice (transaction IDs for idempotency).

**Where periodic integrity sweeps fit:** a sweep that finds IDs appearing more than once is cheap (O(N) over a fixed pool via a hash set), but it's an **auditor**, not a preventer:
- Between the dupe and the next sweep, the duped item can already be traded, consumed or crafted into something else.
- Automatic deletion is dangerous: a sweep can't tell which copy is legitimate without lineage data (transaction log, timestamps), so it risks confiscating an innocent player's item. Real dupe incidents in live games are handled as manual live-ops rollbacks for this reason.

**Conclusion:** atomic transactions = prevention (build first); periodic sweep = detection and alerting, logged for manual review, never auto-deleting.

## Open questions

- Session backend choice (EOS vs. platform-native), and whether host anonymity is worth a broker/relay layer.
- Exact contents and cadence of the host migration snapshot.
- Whether lockstep/rollback is worth prototyping for AI regions, given UE5 determinism risk.
