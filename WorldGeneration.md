# World generation

Procedurally generated maps: PCG on mesh terrain, built per session while the player is in the bunker. The idea and its open feasibility checks are in [Roadmap.md](Roadmap.md) → "World generation"; this doc holds the design. Planned 2026-11-19; nothing is built yet. Engine behavior cited here was checked against the 5.8 source.

## Current direction

- **Prototype first:** PCG places POIs, the server creates managers and AI from them (without behaviors), and the perception field's bones are built alongside. See "Prototype steps".
- **Terrain (2026-11-23): flat Nanite tiles moved by World Position Offset from one shared heightmap.** It works in the editor with full detail at about 0.3 ms VisBuffer. Nanite tessellation turned out to be unusable for large heights. The full map, the headset and collision are still to test; see "Terrain tests".
- How a session goes from the bunker to deploy is in [AI.md](AI.md) → "Session startup and the actor pool".

## Design

### What every machine generates, and what only the server creates

| Content | Who generates it | Replicated |
|---|---|---|
| Terrain, props, foliage | Every machine, locally from the session seed | No. The seed is the only thing sent |
| POIs (as data) | Every machine can; the server's copy is the authority | Only if clients need them (markers, UI) |
| Managers, AI, loot, the gameplay state of destructibles | The server only, from its POI data | Yes, as normal actors and state |
| The perception field (tactical grid) | The server, which runs the AI. Later also a delegated brain's client, from the same inputs | No |

**Rule: graphs never spawn gameplay actors.** Gameplay content leaves a graph as tagged point data, and the server turns it into actors. An actor spawned by a graph is spawned separately on every machine, so it isn't the same actor on the server and on clients.

**Rule: heights are final before the terrain first renders (hard rule, 2026-11-23).** Terrain tiles cache their shadows (Shadow Cache Invalidation Behavior = Static), and cached shadow pages aren't redrawn when the heightmap changes. Without that caching, WPO terrain re-renders its shadows every frame.
- The pipeline writes and uploads the heightmap **before** any terrain tile is registered or made visible. Tiles that already exist, such as the flat base around the bunker, stay hidden until the upload is done.
- **Any later height change** (runtime deformation, regeneration, reload after host migration) invalidates the affected tiles' cached shadows **in the same step** that uploads the new heights. Re-registering the tile component's render state should do it; verify that it clears virtual shadow map pages.
- The same step rebuilds everything else read from the heights: the affected collision sections, the tactical grid's ground layer, and any PCG output that sampled the old terrain.

### The seed and replication

- **PCG doesn't replicate the seed.** `UPCGComponent::Seed` (`int`, default 42, `PCGComponent.h`) isn't a replicated property: `PCGComponent.cpp` registers none. `Generate(bool)` is a reliable `NetMulticast`, but it carries no seed, so it would generate the default map everywhere.
- **A replicated session spec on the GameState:** seed, biome and the two factions. When it arrives (RepNotify on clients; directly on the host), each machine sets `Seed` on its PCG components and calls `GenerateLocal`.
- **One seed per graph:** each graph's seed is a hash of the session seed and the graph's identity, so graphs don't produce correlated layouts from one number.
- **Late joiners and host migration** receive the spec and regenerate the same map. Everything that changed since generation (destruction, captured POIs, loot taken) replicates as ordinary state on top of it.

### Determinism

PCG's randomness comes from seeds (the component's, each node's settings, and each point's), so the same inputs give the same outputs. What can still break it:
- **World queries inside graphs.** Ray hits and overlaps return whatever exists when they run. Graphs may sample the terrain, but never PCG-spawned props or physics state.
- **Completion order.** Partition cells finish in any order. Nothing, ids included, may depend on which cell finished first.
- **Floating point across machines.** The same binary on x64 normally gives the same results. Code that picks a path by CPU features could differ, so verify rather than assume.
- **Mesh terrain** generation has to be deterministic too (unverified).

**Verification:** each machine hashes its output per partition cell (quantized point transforms and mesh ids). After generation, clients send their hashes to the server, and a mismatch is logged with the cell's coordinates. That's both a debugging tool and a safety check. The prototype only logs; what to do about a mismatch is open.

### Generating without freezing the headset

- **The executor is already asynchronous.** Elements run on worker threads, and game-thread work is time-sliced by `pcg.FrameTime`, which defaults to 5 ms per frame (`PCGGraphExecutor.cpp`).
  - At 90 Hz a frame is 11.1 ms, so 5 ms of game thread in the bunker is too much. Lower it to about 1–2 ms during generation and restore it afterwards.
  - `pcg.Runtime.MaxPercentageOfThreadsToUse` defaults to 0.9, which leaves little for the render and RHI threads. Cap it at about 0.5 during generation, then measure.
- **Batches come from partitioning.**
  - A partitioned component splits generation into grid cells (`APCGWorldActor::PartitionGridSize`, default 256 m), each a local component generated as its own task.
  - Hierarchical generation adds coarse-to-fine levels: sites on a large grid, POIs on a middle one, props on a small one.
  - Every local component fires its own `OnPCGGraphGeneratedDelegate`. Whether the original component's delegate fires once all local ones are done needs verifying.
- **Some spikes can't be sliced.** A single task's game-thread work runs in one go: one very large ISM component, spawning heavy actors, creating physics bodies for collision. Keep cells small enough, prefer ISMs to actors, and avoid spawning actors in graphs.
- **Not the runtime generation scheduler.** `GenerateAtRuntime` generates around generation sources (players) by radius. AO needs the whole map before deploy, so use `GenerateOnDemand` and call `GenerateLocal`.
- **Our own work is batched too:** manager creation and pool acquires are spread over frames.
- **Navmesh** is built dynamically after terrain and props, and is part of the world-ready signal.

### POIs and the site tree

**`FPOIId`: a packed `uint32` with the same conventions as `FPurposeEntity`** (see AI.md → "Reference identity"):

```
 31      24 23          12 11           0
[  region  |     site     |     POI      ]
   1 … 255     1 … 4095      1 … 4095
```

- **Region:** a large part of the map, such as a valley.
- **Site:** a place with an identity: a village, a compound, a crossroads.
- **POI:** a spot that managers are homed at: a garrison building, a checkpoint.
- **Zero means "this level itself":** POI 0 is the site, site 0 is the region, and 0 overall is invalid. A POI decodes to its site and region by masking, so strength can be summed up the tree without a separate lookup.
- **The faction isn't in the id,** because a POI changes hands when it's captured. Ownership is data on the POI record.
- **Ids come from the generation data, never from completion order.** The graph orders sites within a region and POIs within a site (for example by position), so every machine derives the same ids without replication. The prototype can simply assign them on the server when POIs register.

**A POI record holds:** its id, location and bounds, its initial owning faction, and its element list (size and attributes for each manager; see AI.md → "Managers and the command hierarchy").

**Getting POIs out of PCG.** The graph emits POI points on a tagged output, with attributes for site, POI index, faction and elements. Two ways to hand them to the server; pick one in the prototype:
- read `UPCGComponent::GetGeneratedGraphOutput()` when the component's generated delegate fires;
- a custom PCG node at the end of the graph that pushes the points to the subsystem. If it touches UObjects, it must run on the main thread.

Faction strength per site and region is AI's concern, in AI.md → "Faction strength by area".

### Prototype steps

1. **POIs, managers and bodies on a flat map.**
   - One non-partitioned graph emits POI points; there's no terrain yet.
   - The session spec on the GameState supplies the seed.
   - The server registers sites and POIs, creates a manager per element, and acquires placeholder bodies (capsules) from a minimal pool.
   - Debug draw ids and faction strength.

   **Done when:** the same seed gives the same POIs and ids, and a different seed gives a different layout.
2. **Partitioned and hierarchical generation with budgets.** Measure frame time on the target headset while generating in the bunker. **Done when:** no frame goes over budget during generation.
3. **Client generation and hashes.** A listen server and one client. **Done when:** every cell's hash matches.
4. **The perception field's bones**, built from the same PCG data (AI.md → "Building the grid from PCG").
5. **Mesh terrain**, once its runtime capabilities are verified.

### Large-map concerns (noted 2026-11-20)

- **Precision is fine at our sizes.** UE5 uses doubles for world positions (Large World Coordinates), and the map is centered on the origin (the four center tiles meet at 0,0,0), which halves the largest coordinate. At 8 km from the origin a float still resolves well under a millimeter, so no origin rebasing is needed. Problems start in the tens of kilometers.
  - Heightmap UVs come from world position: `uv = worldXY / mapSize + 0.5`.
- **World Partition doesn't stream generated content.** WP streams actors saved into the level's cells. Everything PCG and our code create at runtime stays loaded until we remove it. For a 4–8 km map of ISMs and Nanite that may simply fit; measure memory before building our own streaming.
- **No HLODs for generated content,** since WP's HLODs are built in the editor. Nanite covers geometry LOD. Small props need cull distances on their ISMs, so the GPU doesn't cull millions of far instances every frame.
- **Navmesh over the whole map** has a memory and build-time cost. Dynamic generation with tiles is part of the world-ready signal. Navigation invokers (generating only around AI) don't fit, because AI are everywhere.
- **Shadows:** Virtual Shadow Map clipmaps cover the distance (see Rendering.md for the VR caveats).
- **Replication** over a large map depends on relevancy and Iris settings (see Networking.md).

### Terrain tests (first, planned 2026-11-20)

Terrain comes first, because the rest depends on what kind of terrain is possible. Every test runs in a **cooked standalone build on the headset**: PIE loads editor modules and can pass falsely.

**What's one-time and what isn't.**
- Building the heightmap, the collision and the grid's ground layer happens once, during generation.
- The displaced terrain's render cost is paid every frame. Tessellation and displacement put the terrain on Nanite's programmable rasterizer for as long as it's on screen.

**Result (2026-11-23): Nanite tessellation can't carry the large-scale shape.** Tessellation and culling are computed from the flat, undisplaced mesh. At tens to hundreds of metres of displacement that produces holes and VisBuffer spikes to about 20 ms, depending on camera pitch (details under Test 1). At Magnitude 100 (1 m) every problem disappears, and a tile with the height built into its vertices has no performance problem. Tessellation is for small surface detail on geometry that already has its shape.
- **New direction: the large-scale height goes in the vertices.** Nanite meshes can't be built at runtime, so the candidates are:
  - **A non-Nanite runtime mesh per tile**, with heights written at generation and our own LOD (stitched edges or skirts), the way non-Nanite Landscape works. The single-tile Nanite-off test (with auto LODs) approximates its cost.
  - **Flat Nanite tiles moved by World Position Offset**, with `MaxEdgeLengthFactor` above 0 so the flat clusters aren't simplified away. LOD and culling still come from the undeformed mesh, so it may show a milder form of the same problem. Untested; testing next.
    - **How WPO differs from displacement:**
      - WPO moves the vertices the mesh already has, by any vector. Displacement moves vertices that tessellation creates, along the normal, and only with tessellation on. So the tile itself must be dense: about 1 m quads to match the heightmap.
      - WPO goes straight up in world space, so the normal-quantization lean doesn't apply.
      - There's no tessellation, so no depth clamp and no near-plane drop. Clusters that cross the near plane go to the hardware rasterizer, which clips them.
      - It shares two problems with displacement: shading normals don't change, so the slope normal stays, and LOD and culling still use the undeformed mesh with bounds inflated by the material's Max World Position Offset Displacement.
    - **`MaxEdgeLengthFactor`:** the simplifier counts a collapse's error as at least the resulting edge length divided by the factor (`MeshSimplify.cpp:553–567`). Coarser LODs then keep edges at most about that many pixels long on screen.
    - **Virtual shadow maps:** WPO normally invalidates cached shadow pages every frame. The terrain's WPO is static, so set the tile components' Shadow Cache Invalidation Behavior to Static.
    - **First WPO results (2026-11-23, single tile, MapSize 51,200, 524k-triangle tile, `MaxEdgeLengthFactor` 8):**
      - Nanite VisBuffer in PIE is 0.27–0.33 ms, peaking around 0.7 ms while the camera turns. The tessellated version reached about 20 ms at the same kind of angles.
      - **WPO is clamped per axis to the material's Max World Position Offset Displacement** (`MaterialTemplate.ush:4028–4035`). With HeightRange 50,000 and the override at 5,000, everything beyond ±50 m flattened. Peaks were cut off and the low plains sat flat, while the slope-normal shading still showed their detail. Set the override to HeightRange / 2.
      - The first WPO chain took its height from the UV node instead of the center heightmap sample, which produced a single plane tilted about 44°.
      - **With the clamp fixed, WPO shows full detail, as displacement did.** Tile density compared on the single tile:

        | Tile mesh | VisBuffer | ShadowDepths | GPU total |
        |---|---|---|---|
        | 131k triangles (2 m quads) | ~0.30 ms | ~0.3 ms | 2.91 ms |
        | 524k triangles (1 m quads) | ~0.35 ms | ~0.8 ms | 3.54 ms |

        Shadows account for most of the difference. **With Shadow Cache Invalidation Behavior set to Static, the two meshes perform about the same**, so tile density is a choice of visual quality, memory and collision spacing, not frame time. Without Static, WPO makes virtual shadow maps re-render the terrain every frame.
      - **Static shadow caching has a catch at runtime:** cached shadow pages don't update when the heightmap changes. Write the heightmap before the tiles first render, or invalidate their shadows after any later height change, for example by re-registering the tile component's render state.
    - **PCG isn't affected by the choice of terrain method.** PCG can't read GPU-side offsets, so it reads the same CPU height array the heightmap texture is uploaded from, or traces the collision built from it (Test 2).
- Small displacement (about 1 m) can stay on top of the Nanite option as surface detail. The runtime-mesh option has no tessellation, because tessellation requires Nanite.

**Original direction (2026-11-20, superseded above): flat Nanite tiles displaced by a heightmap.** Kept for the reasoning; parts of it still apply, such as one shared heightmap and the tile layout.
- **One flat tile mesh, cooked once, placed as an ISM grid.** Every tile shares the same Nanite data; heights come from the material sampling the heightmap by world position.
- **Not a custom Mesh Partition.** Mesh Partition exists to author and compile unique geometry per section: modifiers, editing, per-section meshes and streaming. Here every tile has the same geometry, so none of that is needed. It's closer to how Landscape works (a shared grid that samples a heightmap), with Nanite tessellation instead of Landscape's LOD system.
- **Why tiles rather than one large flat mesh:**
  - tessellation needs base triangles of a modest size, and one instanced tile gives that density for the memory of a single tile;
  - tiles line up with collision sections, PCG partition cells and tactical-grid tiles.
- **One shared heightmap (decided 2026-11-20).** Every tile samples the same texture by world position, so neighbors read the same texels at shared edges and there are no texture seams to manage.
  - **Size:** at 16 bits per sample, 4096² is 32 MB, 8192² is 128 MB, and 16384² (the size limit) is 512 MB, too much for a VR GPU budget. A 4 km map at 1 m per texel, or 8 km at 2 m, fits comfortably.
  - **No mips needed:** displacement samples one explicit level (MipLevel 0).
- **Finer heights in the center = a second, finer texture, not denser tiles.** With tessellation, a tile's on-screen detail comes from the dicing rate and the heightmap, not from the base mesh, so a denser center tile adds nothing. Far tiles already get coarser triangles automatically.
  - The material samples a higher-resolution texture inside the center region, and the shared one outside it.
  - Both are generated from the same height function, so they agree at the boundary. A short blend band hides any difference.
  - Mixing base densities is what would cause cracks: a dense tile's extra edge vertices don't lie on a coarse neighbor's edge (T-junctions). That only matters on the non-Nanite runtime-mesh route, which would need stitched edges or skirts.
- **Why not WPO instead of tessellation.** Nanite builds its cluster LODs from the undisplaced flat mesh. A flat surface has zero simplification error, so its clusters collapse to very few triangles, and WPO would then only move those few vertices. Tessellation subdivides after the LOD is chosen, so it doesn't have this problem. WPO height displacement only works on a non-Nanite grid with LODs we manage ourselves, the way Landscape does without Nanite.
- **Not Landscape, and not one huge Nanite mesh.** Neither can be created or changed at runtime: Landscape's editing is editor-only, and building Nanite data needs NaniteBuilder.

**Test 1: Nanite displacement in VR.**
- **Setup:**
  - a flat, dense, cooked Nanite grid;
  - a terrain material instance that overrides Enable Tessellation (`bOverride_bEnableTessellation`), so the master material is untouched;
  - displacement from a heightmap written at runtime. Use 16 bits per sample, since 8 bits steps visibly over a terrain's height range.
- **Project state:** `vr.InstancedStereo=True` and `vr.MobileMultiView=True` (`DefaultEngine.ini`), and `r.Nanite.Tessellation` defaults to 1.
- **Check:**
  - both eyes show the same displaced surface, with no stereo mismatch or flicker;
  - no cracks where tiles meet. Shared edges have the same world positions and sample the same heights, but tessellation across separate instances isn't guaranteed to match. With per-tile textures, each needs a border of texels duplicated from its neighbors;
  - no tiles popping at the screen edges. Culling bounds must include the displacement range, set by the material's displacement magnitude and center;
  - frame time on the headset against the same grid undisplaced, and how `r.Nanite.DicingRate` (default 2 px) trades detail for cost.
- **Leave displacement fade (`bEnableDisplacementFade`) off for terrain.** Fade scales displacement toward zero as the surface shrinks on screen, so distant mountains would sink flat.
- **Done when:** the terrain is correct in both eyes and its frame-time cost is known.

**Test 1 editor setup (working in the editor, 2026-11-20; headset and cooked build still to run).**
- **Map:** 4096 × 4096 m (MapSize 409,600 uu), 8 × 8 tiles of 512 m. Tile centers sit at `(i − 3.5) × 51,200` uu, so the map runs from −204,800 to +204,800 and the four center tiles meet at the origin.
- **Tile mesh (working, 2026-11-20):** a Modeling Mode plane built at full size, 51,200 × 51,200 uu, 16 × 16 quads (512 triangles), World Space UV Scale off, Nanite on with Normal Precision 15. It's instanced at scale (1, 1, 1). Changing the mesh meant deleting and re-placing all the tiles.
  - The first attempt, the engine plane (100 × 100 uu) instanced at scale (512, 512, 1), got the height right but leaned every vertex sideways. The cause is below.
  - 512 triangles is a starting point, not a decision. If the headset profiling or the seam checks show problems, try 2k–8k triangles per tile.
  - **Z scale must be 1.** Nanite moves the vertex along its normal transformed by the full instance matrix without renormalizing (`NaniteVertexFactory.ush:512`), so displacement is multiplied by the Z scale. A uniform scale of 512 turned a 500 m height range into about 200 km.
  - **Non-uniform X/Y scale leans all displacement sideways (seen 2026-11-20).** Nanite stores normals octahedrally quantized, 8 bits by default (`NaniteEncode.cpp:1476`). The decode `q × 2/(2ⁿ−1) − 1` (`NaniteAttributeDecode.ush:171`) can't produce exactly 0, so a flat plane's +Z normal comes back tilted by about 1/255 on X and Y. The 512× X/Y scale multiplies that to roughly 2:1 sideways, and every tile's displacement leaned toward (−X, +Y).
  - **Fix (confirmed in the editor 2026-11-20): build the tile at full size (51,200 uu, scale 1) and set the mesh's Nanite Normal Precision to 15 bits** (the maximum). At scale 1 with 8 bits, the lean is about 0.3°, which is still around 3 m sideways at a 500 m peak, enough to put collision visibly out of line with the render. At 15 bits it's about 2 cm. The extra precision costs memory for one tile mesh only.
- **Heightmap:** `Assets/Terrain/T_Heightmap_DiagonalRidge_4096.png`, 16-bit grayscale. A ridge runs corner to corner, so it crosses every tile junction on the diagonal, with a peak on the origin junction. Import with compression Grayscale (stays G16), sRGB off, address Clamp, and **Mip Gen Settings NoMipmaps (or Never Stream)**. The texture streamer estimates mips from mesh UVs, can't see world-position sampling, and could leave only low mips resident under the MipLevel 0 sample. `*.png` isn't covered by LFS yet.
- **Material (`M_NoTexture9Region_Master`, IsTerrain static switch):**
  - UV = `AbsoluteWorldPosition.xy / MapSize + 0.5`. Divide first, then add. `worldXY / (MapSize + 0.5)` gives UVs from −0.5 to 0.5, so the negative quadrants clamp to the texture's edge.
  - Five samples at MipLevel 0, Linear Grayscale: the center one's R goes to Displacement. The neighbours are offset ± Texel Size (from the Texture Property node) and build the slope normal: `dx = (h(+U) − h(−U)) × HeightRange / (2 × MapSize × TexelSize.x)`, the same for `dy` with TexelSize.y, and normal = `normalize(−dx, −dy, 1)`. Displacement doesn't update shading normals, so without this the terrain lights as flat.
  - The switch's False inputs stay empty, for both Displacement and Normal. Any compiled value, even a constant 0, marks the shader map as using displacement (79 → 113 shaders).
- **Material instance (`MI_NoTexture9Region_Terrain`):** IsTerrain on, Enable Tessellation override on, Displacement **Center 0** and **Magnitude = height range in uu** (50,000 = 500 m). `HeightRange` must equal Magnitude, because the graph can't read the material instance's Magnitude.
- **Editor results (2026-11-20):** no seams between tiles, and the slope normal lights correctly on both axes. Stereo and frame time on the headset in a cooked build are still to test.
- **Problem seen at 500 m relief (2026-11-20, editor and PIE):** near the ground, holes appear when looking down and nearby parts vanish, and frame rate falls from 120 to 80 fps when looking up.
  - **Tests (2026-11-23):** raising `r.Nanite.MaxCandidatePatches` and `r.Nanite.MaxVisiblePatches` to 8M changed nothing, so it isn't queue overflow. Raising `r.Nanite.DicingRate` made more triangles vanish close to the camera and fewer far away.
  - **Mechanism, from the 5.8 source:**
    - Tessellation density is computed from the flat, undisplaced base triangle (`NaniteSplit.usf:162–171`, `:228`). Where the displaced surface is far above its base, the ground near the camera is under-tessellated, so each micro-triangle is large on screen.
    - The tessellated software rasterizer doesn't clip. Any micro-triangle with a vertex in front of the near plane is dropped whole (`NaniteRasterizer.usf:1095`). Large micro-triangles near the camera cross the near plane, so they disappear and leave holes. A coarser dicing rate makes them larger, which matches what the test showed.
    - Large micro-triangles are also slow, because each one covers many pixels in a software rasterizer. Cluster bounds inflated by the full displacement on every axis (`NaniteClusterCulling.usf:169`) also keep everything near the camera in view. Either may explain the cost when looking up.
  - **Conclusion:** Nanite tessellation suits displacement that's small relative to the base geometry, and 500 m isn't. The error grows with the distance between the flat base and the real surface.
  - **Next test:** realistic relief with Center 0.5, so the base sits mid-height. For example, Magnitude and HeightRange 10,000 (±50 m), checked at eye height in VR.
  - **Relief results (2026-11-23):** 10,000 and 25,000 both cost fewer frames than 50,000, and about the same as each other. Holes still appear, and only once the camera pitches up or down. With the camera level there are none. Holes are worse closer to the ground. The frame rate halves when the camera pitches just above the surface at a grazing angle and recovers when it looks higher. In Simulate, the frame rate also halves whenever the camera is flown (right mouse held).
  - **Diagnostic results (2026-11-23):**
    - `stat gpu`, Nanite VisBuffer: under 0.8 ms looking level near the ground. 6–7 ms pitched down and 4–5 ms pitched up. **About 20 ms** in two cases: looking up from near the top with the surface just out of view, and pitched about 45° down near the ground. About 9 ms looking straight up.
    - ShadowDepths stays around 0.5 ms, peaking near 1 ms, so shadows aren't the cause.
    - The flying frame drop happens only in Simulate, not in the regular editor.
    - `r.SetNearClipPlane 1` made no difference. That doesn't rule out the near-plane drop: if the oversized triangles extend metres past the camera, moving the plane from 10 to 1 uu changes nothing.
    - **Reading:** the cost is tessellation of geometry that's off-screen or around the camera. Patches pass culling on bounds inflated by the displacement range, and are diced at a rate computed from the flat base near the camera (`GetTessFactors` clamps edge depth to the near plane, `NaniteTessellation.ush:91`). 20 ms is far beyond a VR frame (about 11 ms at 90 Hz for everything), so this form isn't viable for VR as it stands.
  - **Why pitch and height matter (2026-11-23):** tessellation depth is the base point's distance along the camera's forward axis (`NaniteTessellation.ush:89–92`). With Center 0.5, the flat base sits mid-height, so it's above you in low ground and below you near the top.
    - With the camera level, that distance is horizontal, so height doesn't affect it.
    - Pitched toward the flat base (down in low ground, up near the top), base points under the surface you're looking at fall behind the camera plane. Their depth is clamped to the near plane, they're diced as if they were a few centimetres away, and their inflated bounds keep them in view. That matches every measurement so far.
    - Level-camera spots also drop to about 50 fps, with VisBuffer and post processing each at 7–10 ms, for example mid-tile at (−256 m, 768 m), and recover 20 m away. Likely the same cause where the camera sits near the base plane's height; unconfirmed.
    - This is structural, in how Nanite sizes tessellation, so no setting removes it. That supports putting the large-scale height in the vertices.
  - **To narrow it down:**
    - `stat gpu` while reproducing, to see whether the cost is in Nanite rasterization or in shadow depths (virtual shadow maps re-render pages whenever the camera moves, including tessellated terrain);
    - `r.SetNearClipPlane 1` (default 10): if holes shrink, the near-plane drop is confirmed;
    - a single tile displaced in the editor with Modeling Mode's Displace tool, so the height is in the vertices with no tessellation. Compare it as Nanite and as non-Nanite against the tessellated tile. The non-Nanite version is roughly what a runtime-mesh route would cost.
  - **If that isn't enough, the large-scale height must be in the vertices, with displacement adding only metres of detail.** Candidates:
    - **A non-Nanite runtime mesh per tile** with heights written at generation, plus our own LOD with stitched edges or skirts. This is Landscape's non-Nanite approach.
    - **World Position Offset on Nanite tiles with Nanite `MaxEdgeLengthFactor` set above 0.** That setting stops deformable meshes from being simplified away (`EngineTypes.h:3361`), which is what ruled out WPO on flat tiles earlier. LOD and culling still work from the undeformed position, so it has the same scale problem in a milder form. Untested.
- **The Player Collision view mode is misleading here.** It shows the displaced surface, but physics only has the flat tile's collision; displacement exists only on the GPU. Check collision by walking or tracing onto a slope, which is Test 2.
- **Nothing else to enable:** `UseNaniteTessellation()` needs `r.Nanite.Tessellation`, `r.Nanite.ProgrammableRaster` and `r.Nanite.ComputeRasterization`, all on by default in 5.8. Culling bounds grow by the material's displacement range automatically (`NaniteResources.cpp:793`).

**Test 2: collision from the same heights, at runtime.**
- **Candidates:**
  - **A. A Chaos heightfield** (`Chaos/HeightField.h`): 2 bytes per sample, the same representation Landscape uses for its collision. The engine has no general-purpose component for it outside Landscape, so it needs a small custom primitive component that owns the body.
  - **B. `UDynamicMeshComponent`** with complex-as-simple collision and `bUseAsyncCooking`. It's stock, but a triangle mesh costs far more memory than a heightfield.
- **Render and collision must agree.** Sample the heightmap at the collision grid's points, one texel per sample, so displaced vertices and collision triangles interpolate the same values. Otherwise feet float or sink. Check it by tracing down at many points and comparing against the rendered height.
- **Measure:** build time and memory per section, and whether the build can run off the game thread.
- **Done when:** characters walk and trace on the displaced surface everywhere with no visible gap, built in the loading window.

The tactical grid's ground layer reads the same height array directly, with no physics involved (AI.md → "Building the grid from PCG").

## Open questions

- Can mesh terrain be generated and modified at runtime? (See Roadmap.md.) **Source reading (2026-11-20) suggests not in a packaged game:**
  - Mesh Partition's modifiers (noise, remesh, sculpt layers, mesh project) are all in the `MeshPartitionEditor` module.
  - Sections are built during cook into static meshes and collision, and the game loads them as `ACompiledSection` actors (`MeshPartitionCompiledSection.h`).
  - Building Nanite data needs `Developer/NaniteBuilder`, which a packaged game doesn't include.
  - A test in PIE can pass falsely, because the editor modules are loaded there. The check has to run in a cooked standalone build.
  - Fallbacks:
    - runtime meshes without Nanite (`UDynamicMeshComponent`, or `UStaticMesh::BuildFromMeshDescriptions`), at whatever density we choose;
    - authored, cooked terrain tiles that PCG arranges.
- Does a partitioned component's original delegate fire after all its local components finish?
- What happens on a hash mismatch: log, regenerate that cell, or refuse the client?
- Region and site sizes, and whether the region level is needed at all.
- How a POI's faction is chosen: see AI.md → "Open questions (revision 4)" → "Manager".
