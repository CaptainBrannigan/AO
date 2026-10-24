# Physics & Destruction

World interaction (destructible buildings, interactable map props) and the Chaos + Box3D split.

## Current direction

- **Chaos stays the engine's physics** for characters, movement, queries, ragdoll and vehicles.
- **Box3D is a candidate for client-side cosmetic debris only**, conditional on profiling (see "Recommended path"). Not yet committed.
- Box3D has no networking/replication, so its scope is **client-side-only cosmetic simulation**. That's an accepted constraint, not a blocker.

## Design: discrete gameplay state vs. continuous cosmetic execution

A well-established pattern (also how many shipped games handle death ragdolls: the server decides the death, the ragdoll afterwards is cosmetic and unsynced), applied to destruction:
- **Server/Chaos owns the discrete event, not the ongoing simulation.** UE's Destruction workflow pre-fractures Geometry Collections offline (artist-authored fracture patterns). At runtime, Chaos only needs to detect the damage threshold and broadcast one lightweight replicated event (segment ID, fracture pattern ID, impact point/force). It never simulates or replicates the resulting debris. This removes the known bandwidth cost of Chaos Destruction replication: simulating N tumbling pieces never touches the network, whichever engine renders them.
- **Client/Box3D owns the cosmetic execution**: it receives the event and simulates the debris locally and independently per client. Clients' debris piles will diverge, which is fine because nothing gameplay-relevant reads them.
- **Guard rail:** anything gameplay-affecting (bullet penetration, line of sight, cover, nav mesh/pathing) is driven by its own simple, server-authoritative state (e.g. a blocking-volume toggle, a nav-mesh cut region), decoupled from where client-local debris ends up. Never treat Box3D debris position as an authority for anything but visuals.
- **AI tie-in:** a building breaking raises an `Occurrence` (new sightlines/cover/paths), keyed off the server-side fracture event, never off client-local debris. See [AI.md](AI.md) → "World events as occurrences".
- **Debris lifetime:** settled debris needs a per-client sleep/despawn policy (despawn, or convert to simplified static collision once stopped).
- **Blueprint access:** Box3D has no native Blueprint API, but a thin `UFUNCTION(BlueprintCallable)` wrapper around the operations needed (trigger fracture, spawn debris, apply impulse) is the standard way to expose a native library and is worth building anyway as a curated API.

## Research: replacing Chaos with Box3D

**A full replacement of Chaos is not realistically achievable**, and there's no precedent even with more mature libraries. Jolt's UE5 integration (Jolt shipped in Horizon Forbidden West) describes itself as "a bolt-on integration and not a replacement for the existing chaos physics solver." Chaos is load-bearing across `FBodyInstance`/`UPrimitiveComponent`, `CharacterMovementComponent`'s sweeps, the scene-query API (traces/overlaps used everywhere), Chaos Destruction and Cloth (no equivalent in a generic rigid-body library), PhysicsAssets/ragdoll, vehicles and networked physics prediction. `Box3DUnreal` follows the same pattern: Box3D runs as a separate simulation island, with objects marked static/invisible to Chaos and no Chaos↔Box3D interaction.

**Maturity:** Erin Catto's own announcement describes Box3D as **alpha, pre-v0.1**, with character movement, ghost-collision mitigation and joint solver work listed as planned, not shipped. `Box3DUnreal` has limited editor tooling and **no multiplayer replication**.

**Two efforts often conflated:**
1. Antonio Lattanzio's public `Box3DUnreal` plugin: what's actually installable. Per its README, an early-stage parallel/opt-in simulation, regardless of the promotional "I replaced almost my entire Unreal Engine physics pipeline" framing.
2. Erin Catto's own studio integration for "The Legend of California": much deeper (bypassing Blueprint, hooking a custom scripting system/animation framework/ECS). It proves a deep replacement is achievable with enough engine-source control, but it's proprietary to his studio's customized engine and not reusable.

Catto built Box3D targeting "large open worlds with server authority", which overlaps with this project's direction. Worth watching as it matures.

## Recommended path

1. **Profile Chaos's own destruction/debris cost first** at representative scale, including often-unexplored tuning: simplified collision shapes for debris, sleep/wake thresholds, reduced solver substep/iteration counts, broadphase settings. Confirm Chaos Destruction replication is actually the bottleneck before adding a second engine.
2. **Prototype the discrete-event/cosmetic split** on one destructible structure end to end (server fracture event → client-local Box3D debris). Validates the data contract (what crosses the network per break) and that the guard rail is workable.
3. Follow `Box3DUnreal`'s integration pattern (mark objects "No Collision" in Chaos, simulate independently) rather than building from scratch. Leave character movement, ragdoll and vehicles on Chaos.

## Open questions

- Is Chaos Destruction replication actually the bottleneck at target scale? (step 1)
- Exact contents of the per-break replicated event.
- When Box3D is mature enough (post-v0.1) to depend on.

Sources: [erincatto/box3d](https://github.com/erincatto/box3d), [alattanzio/Box3DUnreal](https://github.com/alattanzio/Box3DUnreal), [Jolt UE5 integration discussion](https://github.com/jrouwe/JoltPhysics/discussions/1738), [Epic Dev Community thread on swapping UE5 physics engines](https://forums.unrealengine.com/t/how-to-change-unreal-5-physics-engine-chaos-to-physx-jolt-bullet/1722711), [Announcing Box3D (box2d.org)](https://box2d.org/posts/2026/06/announcing-box3d/), [80.lv coverage of Box3DUnreal](https://80.lv/articles/try-this-box3d-integration-for-unreal-engine-5)
