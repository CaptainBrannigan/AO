# Roadmap

High-level direction and phasing only. Detail lives in the system docs (see [README.md](README.md)).

## Phase 0: VR viability (current)

Prove the performance premise before building gameplay. → [Engine.md](Engine.md)
- Decide target hardware and refresh rate.
- Fix renderer config, strip unused plugins.
- Convert to a C++ project and enable Iris replication with push model.
- Run the viability test: static mesh counts, AI render counts, AI logic counts, combined, polycount.
- Outcome: concrete AI/prop/polycount budgets that the design works within.

Alongside: define the "why play?" (see [Vision.md](Vision.md)).

## Networking

- Player-hosted listen servers, matchmaking instead of a server browser, no anti-cheat product. → [Networking.md](Networking.md)
- Iris replication from the start, with code kept working on generic replication while Iris is experimental.
- Host migration: Phase 1 graceful reload first; relay-based seamless migration only if needed.
- Community moderation tooling plus baseline server-side sanity checks.
- Inventory: atomic per-item ownership transfer first, integrity sweeps as an auditor.

## Rendering & art direction

- Textureless, geometry-driven art style as the main VR performance lever. Color regions on a shared material: **baseline achieved**. → [Rendering.md](Rendering.md)
- **Next:** polycount scaling test on the target headset to set character and prop budgets.
- Single-material / single-draw-call rendering across the scene. **Needs hands-on help to understand and implement when ready.**
- Character composition: merged skeletal bodies + attached static-mesh kit.
- Nanite as a normal-map replacement: a visual upgrade to test after the non-Nanite baseline.

## AI

- Land the Purpose system fixes (load-balancer bug and fixed-sleep throughput cap first). → [AI.md](AI.md)
- Separate perception from decision before scaling up AI density.
- Distance-based swap to baked-animation bodies for distant AI.
- Delegated decision-making: the first player in an area runs that area's AI brains, the host executes. Measure host CPU vs. upload bandwidth first.

## Physics & destruction

- Profile Chaos destruction cost first; then prototype the Chaos-event / Box3D-cosmetic split on one structure. → [Physics.md](Physics.md)

## World generation (idea, 2026-11-11, unverified)

Procedurally generated maps on mesh terrain, built per session while the player prepares in the base. Design and prototype steps: → [WorldGeneration.md](WorldGeneration.md) (planned 2026-11-19).

- **Mesh terrain:** the 5.8 source has it as experimental plugins: `MeshPartition` ("large-scale mesh authoring through spatial partitioning, non-destructive modifier editing, platform-adaptive runtime representations"), `MeshTerrainMode` (editor tools) and `PCGMeshPartitionInterop`. It reads as an extension of Nanite and landscape that PCG can deform.
- **Flow:** the map starts as a completely flat base with the player's base on it. The player spawns in the basement and can't see the map. While they move through the base preparing to infil, the map generates. When it's done, the infil helicopter becomes available and the player enters the match. The map stays for as long as that session is open, so every new session gets a different map.
- **Perception data for free:** PCG deforms the terrain and places the props, so the tactical grid for AI perception can be generated in parallel from the same data, exactly as it is placed. That would replace the capture bake for generated maps. → [AI.md](AI.md) "Building the grid"
- **Determinism:** PCG can be deterministic, so each client could generate the map itself from a shared seed, as long as everything is driven by that seed.
- **Hard limitation, the one-material setup:** unless face region IDs can be reassigned at runtime, generated terrain can't be recolored for dirt paths, gravel and the like. → [Rendering.md](Rendering.md) "Open questions"

**To verify first:**
- Whether Mesh Partition can be modified at runtime at all. Its Boolean and Mesh Project modifiers sit in the editor module, so generation may be editor-only as shipped.
- Whether a generated terrain mesh can carry vertex colors (region IDs) written at generation time.
- Generation time on the target hardware while the player is in the base, and whether per-client generation stays identical across machines (float determinism).
