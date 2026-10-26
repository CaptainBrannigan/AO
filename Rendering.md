# Rendering & Art Direction

Art direction and rendering: the textureless color-region style, character composition, material animation, Nanite, VR engine settings, and polycount testing. Mesh authoring steps are in [MeshAuthoring.md](MeshAuthoring.md).

## Current direction

- **Textureless, geometry-driven art style** is the primary lever for VR performance headroom: solid color regions + baked AO/shading + geometry-driven detail instead of texture maps. Reference point: MOHW/BF3-era fidelity, which runs far better than most native VR titles.
- **Color regions on a single shared material: baseline achieved** (see "Color regions").
- **Keep skeletal meshes to a minimum.** Attach kit as static meshes where possible; merge modular skeletal parts at runtime (see "Character composition").
- **Single-material / single-draw-call rendering** across most of the scene (one or a few shared master materials driven by vertex color and per-instance data, so the renderer can batch and instance). This needs hands-on help to understand and implement when the project is ready for it.
- **Nanite as the normal-map replacement** is a visual upgrade to test later, not a feature dependency (see "Normal maps and Nanite").

## Textureless art style

**Core idea:** rather than reducing texture resolution, eliminate texture sampling for detail almost entirely. Base color comes from solid color regions; visual identity comes from actual geometry (e.g. a bag's buckle, straps and buttons modeled as real shapes). VR's frame budget is much tighter than flat-screen, so texture sampling and ALU cost that's secondary on a flat-screen target is worth eliminating outright here.

Simply *reducing* texture resolution mostly saves VRAM, not per-pixel cost. *Eliminating* sampling removes texture fetches from every pixel and, more importantly, removes the texel-density tension that forces many unique materials. That's what makes single-material batching practical.

**What can be baked into vertex data instead of textures** (vertex color, or the otherwise-unused UV channels as extra data lanes):
- **AO**: baked once, read for free (vs. Screen-Space AO, which costs GPU time every frame).
- **Cavity**: the same kind of occlusion term as AO; fold into the same channel.
- **Roughness / Metallic**: usually vary coarsely, so vertex resolution is enough. Sourcing them from vertex color is strictly cheaper than textures and needs no extra passes: they're just more scalar inputs to the same per-pixel lighting evaluation.
- **Static lighting**: for geometry that never moves and isn't hit by dynamic lights, the entire static lighting contribution can be baked into vertex color (classic vertex lighting). Movable objects (characters, AI, held items) still need live lighting.
- **Emissive mask, SSS/thickness**: for glow accents and cheap fake subsurface, where regions are large enough for vertex resolution.

**Per-region variation without extra draw calls** (e.g. metal buckle vs. fabric bag body):
- **Vertex-data-driven, single material, single draw call (preferred):** a region ID per face selects per-region values in the material. The whole mesh stays one material and one draw call.
- **Separate material slots:** simpler to author, but each slot is generally its own draw call. Acceptable only where draw-call count doesn't matter for that mesh.
- **Crisp boundaries:** use the **Face Corner** attribute domain in Blender. UE splits vertices at boundaries automatically on import (as at UV seams), so no manual topology cuts are needed.

## Color regions

**Baseline achieved:** per-region colors on a skeletal mesh (the UE mannequin) with **no textures**.
- Region ID stored per face corner in vertex color **R**, decoded in the material (`round(R × 5)` for the 0.2-step convention), mapped to a per-instance palette via **Custom Primitive Data**.
- The whole character uses one material slot, one draw call and one shared material instance, and can be recolored per instance (team colors, uniform variants, AI variety).
- **Unset Custom Primitive Data reads as 0 (black).** A parameter with *Use Custom Primitive Data* ticked compiles to a raw read of the component's data slot (`MaterialExpressions.cpp`: `Compiler->CustomPrimitiveData(...)`), so its default value and any material-instance override are **ignored** in the shader. Anything without per-component data set (foliage, PCG/ISM instances, props nobody configured) renders black. The master material therefore needs a second palette of normal vector parameters (set per material instance) behind a static switch (`UseCPDPalette`), used for instanced meshes. `PerInstanceRandom` (Lerp A = 1.0, B = variation) then adds per-instance variation; it returns 0 on non-instanced meshes, which leaves them at full brightness.
- **Gamma gotcha:** vertex colors arrive sRGB→linear converted. Region values are pre-compensated in Blender so the material needs no correction. Tables and checklist are in [MeshAuthoring.md](MeshAuthoring.md).

### How many regions?

- **Practical limit: 9 per mesh** when recoloring per instance. Custom Primitive Data holds 36 floats = 9 RGBA colors per component. Encoding precision (8-bit) isn't the constraint. Index 0 is a real region (the default every face starts as).
- Current convention (step 0.2) gives 6 regions; the step-0.125 convention gives 9 at a tighter rounding margin.
- Beyond 9: a Material Parameter Collection (global palettes, not per instance) or Dynamic Material Instances (per instance, but breaks the shared instance and draw merging).

### Main + shaded (same color, darker areas)

Vertex color **B** holds a per-face or per-vertex **darkening amount** (0 = none, 1 = max): `BaseColor = palette[region] × (1 − B × ShadeStrength)`, with `ShadeStrength` a constant or a palette slot's alpha.
- **Tones don't need their own palette slots.** Lighter and darker versions of one color share a slot; separate slots are only needed for parts that must be recolorable independently.
- Encoding as darkening means unpainted (0) = unshaded: no setup step, and 0 isn't affected by gamma.
- Face Corner domain = hard two-tone bands (cel/graphic look); Point domain = soft gradients (needs vertex density where the gradient changes).
- Recoloring a region automatically recolors its shaded areas.

### AO without textures

AO is baked into vertex color **G** (Cycles can bake directly to a Color Attribute).
- Point domain for smooth occlusion; needs vertex density in creases, which fits the higher-poly direction. On skinned meshes it's baked in the bind pose.
- **UE caveat:** the material's Ambient Occlusion input only affects **indirect** light, so it's subtle in mostly direct-lit scenes. For a visible stylized result, multiply AO into Base Color. At that point AO and the shade channel do the same job, and one channel may cover both.

## Character composition

**Why it matters:** each mesh component draws at least once in every pass it appears in (depth prepass, base pass, each shadow cascade, velocity). A 4-part character with ~5 passes is ~20 draws instead of ~5. Skeletal meshes don't batch with each other (each has its own bone buffer), and each also costs a transform update, culling, bone-buffer upload and skinning dispatch. With dozens of AI this lands on the render thread, which is already tight in VR. Negligible for one character; significant at DMZ-style density.

**Direction:**
- **Body (+ clothing that deforms):** author modular parts if needed, but **merge into one skeletal mesh at spawn** (Skeletal Mesh Merge / `FSkeletalMeshMerge`, or Epic's **Mutable** plugin). One shared master material means the merged result is one section, one draw per pass.
- **Rigid kit (helmets, weapons, packs):** **attached static meshes**. Identical static meshes across characters can merge into instanced draws while keeping per-instance colors and animation via Custom Primitive Data. See [AI.md](AI.md) → "Rendering AI at scale" for the AI-specific side, including baked-animation bodies for distant AI.
- Keep a part as a separate skeletal mesh only if it truly needs its own palette or must change mid-match.
- **Turn off shadow casting on small parts** (eyes, mouth, small gear). They contribute almost nothing visible but add a draw to every shadow cascade.

**Shared palette on merged meshes:** a merged mesh has one primitive, so one 9-slot palette. Region indices are planned **across the whole kit**, not per part, and every body/clothing mesh uses indices from its band. Example plan:

| Indices | Use |
|---|---|
| 0–3 | Skin, hair, eyes, mouth |
| 4–6 | Clothing (tones via the shade channel) |
| 7–8 | Gear/accessories on the merged mesh |

Attached static meshes are separate primitives with their own 9 slots, so rigid kit doesn't consume this budget.

## Material animation (World Position Offset)

**Use case:** small moving parts on attached static meshes (e.g. a weapon's bolt cycling on fire) animated in the vertex shader instead of with bones or moving components.

**Approach:**
- **Drive it with Custom Primitive Data, not a Dynamic Material Instance.** A DMI gives each weapon its own material instance, undoing the shared-instance setup and preventing draw merging. Custom Primitive Data lives in the GPU scene, so merging still works: e.g. 50 AI holding the same rifle can render as one instanced draw per pass, each with its own colors and animation state. Other per-component differences can still break merging, so confirm in a profiler capture.
- **Let the GPU run the animation.** On fire, write **one** Custom Primitive Data float: `FireTime = GetWorld()->GetTimeSeconds()`. In the material: `cycle = saturate((Time − FireTime) × rate)` → shape with a curve/sine → × local direction → **World Position Offset**. One CPU write per shot, instead of a timeline updating a parameter every frame.
- **Isolating the moving part:** reuse the region ID (`region == boltRegion`, if the part is one color), or a mask in a spare channel (A). With several moving parts, the region index can select a different offset per part.

**Caveats:**
- The **36-float budget is shared** between palette and animation values (e.g. 6 colors = 24 floats + a few animation floats).
- **Bounds:** vertices moved outside mesh bounds can be culled incorrectly. Set the material's *Max World Position Offset Displacement* to cover the largest movement.
- **Visual only:** collision doesn't move. Fine for bolts and triggers.
- **Rotations don't update normals:** sliding parts are fine; rotating parts (triggers, hammers) light slightly wrong unless the normal is rotated too. Usually unnoticeable at that size.
- **Runs in every pass** the mesh draws in (depth, shadows) so geometry matches. Negligible for small parts.
- **Time base:** the material's Time node and `GetTimeSeconds()` must be the same clock (game time). Verify in testing. Float time loses precision over very long sessions; wrap or rebase it if matches run for hours.

## Normal maps and Nanite

**Normal maps are the biggest visual loss** of the textureless style. What adding them back costs:
- Direct: one texture sample, unpacking the normal, and a TBN transform (extra interpolated tangent data + a per-pixel matrix multiply). Modest in absolute terms on PCVR, but proportionally large against this pipeline's low baseline, and larger still on standalone VR.
- Quieter: reintroduces the texel-density tension (close-up objects need more resolution than background props), pulling back toward more material instances and away from draw-call consolidation.
- Hybrid fallback: normal maps only on scrutinized objects (held weapons, hands), using a small shared library of tileable normal-detail textures rather than unique per-object maps.

**Nanite as the replacement (visual upgrade, to test later):** represent fine surface detail as real geometry via Nanite rather than faking it with normal maps.
- **Stronger fit for VR than it first appears:** normal maps fool shading-based depth cues but not stereo depth perception. Both eyes report zero real parallax for a normal-mapped bump, so faked detail can read as subtly flat in VR. Real geometry doesn't have this problem.
- **Risks to resolve first:**
  - Nanite's VR/stereo support has varied across UE5 versions. Verify on the engine version in use.
  - No known mobile/standalone Nanite support. If Quest-class hardware is a target, Nanite may only work for a PCVR tier, with a fallback for standalone.
  - Best for static, opaque, non-deformable geometry. Masked materials cost more in Nanite, translucency isn't supported, and skinned Nanite is newer and more constrained. Test on static opaque props first.
  - Runtime-built meshes can't use Nanite (one reason building levels from another game's install files was dropped).
  - **Nanite needs the deferred renderer in practice (found 2026-10-25):** it only worked after turning forward shading off. That gives up forward shading's VR benefits, notably **MSAA**, which is forward-only; deferred would need TAA/TSR, which costs more and smears under head motion.

**Test result, trees-only scene (2026-10-25):** forward/no-Nanite vs. deferred + VSM + Nanite. Frame pacing was the same (~119.8 fps at 120 Hz). Game thread about the same (1.28 vs 1.32 ms). **Render thread +1.05 ms** (scene render 1.25 → 2.30 ms), of which Nanite itself (`InitNaniteRaster`) was ~0.25 ms; the rest was VSM shadow depths (+0.25 ms), visibility and render-graph overhead. GPU +0.4 ms: geometry got cheaper (base + prepass 1.47 → 0.86 ms, plus ~0.4 ms Nanite raster), but deferred lighting (0.59 ms) and VSM (~0.5 ms) more than cancelled it. **Corrected result, 4× map + foliage (2026-10-25), after fixing the foliage material's missing *Used with Instanced Static Meshes* flag:** the earlier Nanite runs were invalid because the packaged build drew the foliage through the traditional path (see MeshAuthoring.md → usage flags). Fixed, deferred + VSM + Nanite at 4× holds **119.9 fps, 0.05% frames over 12.5 ms**, render thread scene render **3.40 ms** (visibility 1.59 ms), GPU passes **~3.5 ms**, traditional draws 0.10 ms, Nanite raster 0.72 ms. The same 4× scene in forward without Nanite: 85 fps, 40.7% missed frames, render thread 9.95 ms, GPU ~9.3 ms. **At large-map foliage density, Nanite (with deferred + VSM) is decisively better.** The 1× Nanite runs had the same bug and should be redone for a fair small-scene comparison. Remaining GPU costs: deferred lighting ~0.62 ms, post-processing ~0.57 ms, VSM ~0.6 ms, Nanite raster + base pass ~1.0 ms.

**Dynamic shadows on, same 4× Nanite scene (2026-10-25):** essentially unchanged, about +0.1–0.15 ms GPU (deferred lighting 0.62 → 0.68 ms, direct lighting 0.54 → 0.60 ms, VSM depths/render +0.03 ms each); still 119.8 fps, 0.07% over 12.5 ms. The previous "static" runs were already lighting and shadowing the Nanite meshes dynamically (Nanite meshes don't receive baked shadows), so most of the cost was already there. **Baseline after this:** game thread ~1.3 ms, render thread ~2.8–3.4 ms, GPU ~3.5 ms at a locked 120 Hz, leaving ~4.5–5 ms per thread for AI/gameplay/content. The render thread came out *lower* (scene render 3.40 → 2.77 ms) with the camera unchanged. The entire drop is `WaitForVisibilityTasks` (1.58 → 0.93 ms), the render thread waiting on parallel visibility work. The light's **Cast Static Shadows was turned off** for this run, which plausibly removes shadow-relevance work from those tasks (not provable from render-thread-only data). **Net for Nanite scenes:** static shadows off + dynamic shadows on ≈ −0.6 ms render thread, +0.1 ms GPU. Keep using a fixed capture route (same start pose, same turn, bookmarks) for comparisons.

**Nanite does not require Virtual Shadow Maps.** The 5.8 source renders Nanite geometry into traditional shadow maps too (`RenderShadowDepthAtlasNanite` in `ShadowDepthRendering.cpp`, `r.Shadow.Nanite`). VSM is Epic's recommended pairing (sharper, scales with dense geometry) but optional; deferred shading is the actual requirement found. "Nanite + traditional shadows" is a configuration to measure.

**VSM tuning levers, if VSM is used** (from [StraySpark: VSM optimization for open worlds, UE 5.7](https://www.strayspark.studio/blog/virtual-shadow-map-optimization-open-worlds-ue5-7); console/handheld-focused and silent on VR/forward shading, so the levers apply but the numbers don't; some figures promote the author's own tool):
- Foliage WPO: disable WPO in shadows for small foliage; WPO disable distances (~30 m mid plants, ~80 m trees). Animated geometry invalidates cached shadow pages every frame.
- Shadow casting off on decorative animated props, water and Niagara; limit shadow-casting skeletal meshes. **Dense AI + VSM needs attention**, since every animated character invalidates its pages each frame.
- `r.Shadow.Virtual.Clipmap.LastLevel` ~20 with fog hiding the horizon; `r.Shadow.Virtual.UseAsync 1`; `r.Shadow.Virtual.MaxPhysicalPages` set once, globally.
- `ResolutionLodBiasDirectionalMoving`: careful in VR, where the head always moves, so it may cause visible shimmer.
- They advise against Nanite for <500-tri meshes and leaf-card foliage.
- Debug: `stat VirtualShadowMapCache`, `r.Shadow.Virtual.Visualize 1`; >5–10% pages invalidated per frame = an invalidation storm.
- Their "PC high-end" VSM budget (1.5–2.5 ms) is 20–30% of a 120 Hz VR frame, so it doesn't apply to us as-is.

**Conclusion so far:** at this polycount Nanite's own cost is small; the "Nanite tax" is mostly the deferred + VSM setup it requires. Open: isolate with deferred/no-Nanite and deferred/VSM-off/Nanite runs, then find the polycount where Nanite pays off.

## Performance

### Measured material stats (color-region master material, Default Lit)

- Base pass PS: ~205 instructions; VS: ~139. Lightmap permutation: ~257 PS / ~221 VS.
- 2–3 samplers / 3 estimated texture lookups. These are **engine-side** (lightmap/volumetric lightmap data and lighting lookup tables); the material samples no textures.
- Our graph's own share is roughly 15–20 instructions. The rest is UE's fixed Default Lit base-pass template, so graph tweaks have limited headroom; engine/project settings are the bigger lever.
- 44 shader permutations, driven by usage flags/vertex factories. Affects compile time and memory more than runtime.

### Optimization candidates

**Graph-level (small, free):**
- **Remove the Power (gamma) node** by pre-compensating region values in Blender (tables in [MeshAuthoring.md](MeshAuthoring.md)).
- **Move color selection to the vertex shader** via a Vertex Interpolator. Exact, not approximate, because regions are per face and vertices are split at boundaries. ⚠ **Trades against the higher-poly goal:** it pays off when pixels far outnumber vertices (a VR character up close) and loses value as vertex density approaches pixel density. Re-evaluate once polycount tests set real targets.

**Material settings:**
- **Fully Rough** where specular isn't needed (props, distant LODs). Removes specular evaluation and its lookup-table sample.
- Untick unneeded **usage flags** (Morph Targets, Instanced Static Meshes, Niagara…) to cut permutation count.

**Project-level (affects every material):**
- **Forward Shading + MSAA** for VR. Base-pass instruction count rises because lighting moves into it, but the GBuffer and deferred lighting passes go away. Only frame-time profiling shows this trade.
- Disable unused engine features: base-pass velocity output (not needed without TAA/motion blur), volumetric fog, global clip plane, extra sky/fog features.
- **Unlit + fully baked vertex lighting for static props.** Not for characters, but props are most of the scene's surface area and Unlit is a small fraction of Default Lit's cost.

### VR engine feature audit

Project-wide renderer settings (Instanced Stereo, Forward + MSAA, Lumen/ray tracing/VSM off, etc.) are tracked in [Engine.md](Engine.md) → "Renderer config audit". Rendering-specific items not covered there:
- **Nanite** is independent of Lumen and may be worth keeping for PCVR-tier geometric detail even with Lumen off (see "Normal maps and Nanite"); the standalone support question decides it project-wide.
- Cap dynamic shadow-casting lights in favor of baked lighting; audit post-processing for VR-inappropriate passes (SSR, heavy bloom, chromatic aberration, film grain).
- Foveated rendering (fixed or eye-tracked, with Variable Rate Shading) if the target headset supports it.
- LOD/culling tuning for VR's effective FOV.

### Polycount scaling test (next step)

The color-region baseline removes texture sampling and material proliferation from the budget. The open question is how much higher polycounts can go as a result, since geometry-driven detail is the art direction's main visual lever.

1. Build a test scene with N instances of the region-colored character (and later static props) at stepped triangle counts, e.g. 1×, 2×, 4×, 8× the mannequin's base count.
2. Compare against an equivalent textured baseline at the same counts, to quantify what dropping textures buys.
3. Profile on the **target headset** with `stat unit`, `stat gpu` and GPU Visualizer. Watch where geometry cost lands:
   - **Depth prepass** and **shadow depth passes** scale with triangle count; shadows are often first to blow up.
   - **Skinning** scales with vertex count on characters.
   - **Base pass VS** scales with vertices; this is where the vertex-interpolator trade shows up.
4. Record the polycount at which frame time crosses the VR budget (~11 ms at 90 Hz) for characters and for props. Those numbers become the art budget.
5. Nanite (static props, PCVR) is a separate follow-up once there's a non-Nanite baseline.

## Open questions

- Polycount budgets for characters and props (from the scaling test).
- Whether AO and the shade channel should be merged into one channel.
- Which Nanite outcome: PCVR-only tier vs. not used.
- Whether vertex color alpha arrives without gamma conversion (it would be a cleaner home for the region ID).
