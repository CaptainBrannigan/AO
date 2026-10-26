# Project Docs

## How these docs are organized

Docs are organized **by system**. Each system doc holds everything about that system (design, behavior, performance, networking, open questions), so working on one system means reading one doc. Cross-cutting concerns appear as a section inside each affected system doc rather than as their own doc. For example, AI performance lives in `AI.md`, not in a separate performance doc.

Three docs are deliberately not system docs:
- **Roadmap**: high-level direction and phasing only. It links into the system docs for detail.
- **How-to / checklists** (e.g. `MeshAuthoring.md`): step-by-step reference for repeated work.
- **This README**: the index.

### System doc layout

Each system doc follows roughly the same order, skipping sections that don't apply:
1. **Current direction**: decisions made so far.
2. **Design**: how it works or is meant to work.
3. **Performance**: measurements, known costs, optimization candidates.
4. **Networking**: what crosses the wire and who is authoritative.
5. **Open questions**: unresolved items. Undecided ideas stay here until they're decided.

When a topic spans systems, keep the full write-up in the system that owns it and link to it from the others. Don't duplicate it.

## Index

| Doc | Covers |
|---|---|
| [Vision.md](Vision.md) | Theme, pillars, and the open "why play?" question |
| [Roadmap.md](Roadmap.md) | Direction and phasing across all systems |
| [Engine.md](Engine.md) | Project config, plugin stripping, frame budgets, profiling method, VR viability test |
| [AI.md](AI.md) | Purpose (utility AI) system: design, behavior, performance fixes, networking, rendering AI at scale |
| [Rendering.md](Rendering.md) | Art direction and rendering: textureless color regions, character composition, material animation, Nanite, VR engine settings, polycount testing |
| [Networking.md](Networking.md) | Session model, matchmaking, host migration, community moderation, inventory integrity |
| [Physics.md](Physics.md) | Destruction and world interaction (Chaos + Box3D split) |
| [MeshAuthoring.md](MeshAuthoring.md) | Checklist for authoring meshes for the color-region material |
| `Tools/Profiling/` | Trace analysis scripts (`analyze_trace.ps1`); usage in Engine.md → "Analysis tooling" |
