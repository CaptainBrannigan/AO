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

## Physics & destruction

- Profile Chaos destruction cost first; then prototype the Chaos-event / Box3D-cosmetic split on one structure. → [Physics.md](Physics.md)
