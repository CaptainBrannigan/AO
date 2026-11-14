# Mesh Authoring Reminders

Checklist for authoring meshes that use the textureless color-region material (see [Rendering.md](Rendering.md) → "Color regions").

> ## ⚠ IMPORTANT: region IDs must be pre-compensated for gamma
>
> Vertex color values are gamma-converted (sRGB → linear) between Blender and the shader. A value typed as `0.2` arrives in the material as roughly `0.033`. If you type plain step values (0.2, 0.4…), every region decodes **one index too low**.
>
> **Always type the pre-compensated values from the tables below**, not the plain step values. The material then decodes with no Power node.
>
> If FBX export settings or the attribute type ever change, re-run the debug-color test before trusting these values.
>
> **This applies to skeletal meshes only (found 2026-11-13, UE 5.8 source).** The FBX importer decodes vertex colors sRGB → linear for every mesh type. The **skeletal** build stores the result as is (`ToFColor(false)` in `SkeletalMeshLODImporterData.cpp`), so the material receives the linear value and needs the pre-compensated tables. The **static mesh** build re-encodes to sRGB (`ToFColor(true)` in `StaticMeshBuilder.cpp`), so the material receives the **file value unchanged**. A static mesh therefore needs plain values: region k = k/8 (or k/5), and Shade as the linear darkening amount (no pre-compensation). With pre-compensated values a static mesh decodes regions 1–3 as 3–5 and its shading comes out too dark. Import characters and clothing as skeletal meshes.
>
> The passes add-on writes either encoding: **Import in UE as** (Skeletal / Static), in the Region IDs panel and under Geometry > Output (0.1.2). Apply IDs writes RegionID for the chosen type and re-encodes an existing Shade to match, so a mesh can be switched without rebuilding geometry. Each mesh records the type it was written for (`aop_vc_encoding` on the mesh data, carried to the reduced and export copies), and the panel warns when it differs from the setting. Static values: 0.125 convention `0, 0.125, 0.25 … 1.0`; 0.2 convention `0, 0.2 … 1.0`.
>
> **Nanite skinned meshes lose vertex colors entirely** in UE 5.8: the Nanite skeletal build doesn't copy them (`// TODO: Nanite-Skinning` in `SkeletalMeshBuilder.cpp`), so every face decodes as one region. Static Nanite meshes keep them. Fixed in the 5.9 dev branch; the project waits for 5.9. See AI.md → "AI rendering test plan".

## Region ID encoding (R channel)

Index 0 is a normal region, the **base/default region**. A new color attribute defaults to black (0), so every face starts out as region 0 and only the other regions need assigning. Make region 0 the mesh's largest or most common area (e.g. the body) so that the default is the sensible one. A forgotten face will silently take region 0's color rather than standing out, so check the viewport attribute view before exporting.

### Current convention: step 0.2, material multiplier 5 (up to 6 regions)

| Index | Type in Blender (R) |
|---|---|
| 0 | 0.0 (default, no assignment needed) |
| 1 | 0.4845 |
| 2 | 0.6652 |
| 3 | 0.7977 |
| 4 | 0.9064 |
| 5 | 1.0 |

### Alternative: step 0.125, material multiplier 8 (up to 9 regions)

Uses all 9 Custom Primitive Data color slots. The rounding margin is tighter (±0.0625 vs ±0.1), so verify with the debug material.

| Index | Type in Blender (R) |
|---|---|
| 0 | 0.0 (default, no assignment needed) |
| 1 | 0.3886 |
| 2 | 0.5371 |
| 3 | 0.6460 |
| 4 | 0.7354 |
| 5 | 0.8125 |
| 6 | 0.8808 |
| 7 | 0.9429 |
| 8 | 1.0 |

Pick **one** convention for all meshes so a single master material decodes every mesh. The multiplier in the material must match it.

## Blender checklist

- [ ] One color attribute named `RegionID`, data type **Color** (float), domain **Face Corner**. Face Corner gives crisp boundaries; UE splits vertices automatically on import.
- [ ] Do not make one attribute per region; all regions go in the single `RegionID` attribute.
- [ ] Other per-vertex data (`Shade`, `AO`) goes in **its own attribute** and is packed into one export attribute at the end (see "Packing for export"). Set Attribute writes all four channels at once, so painting shade directly into `RegionID` would overwrite the region values.
- [ ] Whichever attribute you're editing must be the **active** attribute. Set Attribute and Vertex Paint write to the active one.
- [ ] Assign in Edit Mode → **Face select** → select a region's faces → **Mesh → Set Attribute**.
- [ ] Type values in the color picker's **RGB** fields. **Never use the Hex field**, because it's gamma-corrected.
- [ ] Overlapping selections: the last write wins. Assign small regions (eyes etc.) **after** the larger regions that contain them.
- [ ] Verify in the viewport: Solid shading → **Color → Attribute**. Each region should show its own shade. Black faces are region 0, so check that none are faces you meant to assign elsewhere.
- [ ] **One material slot** for the whole mesh (one draw call / one section). The slot name is all that matters; the Blender material itself doesn't export.
- [ ] Don't delete or rename skeletal vertex groups. They're bone weights and skinning depends on them.

## Generating regions from an existing texture (script)

The **AO Color Regions** Blender add-on ([Tools/Blender/ao_region_tools](Tools/Blender/ao_region_tools), engine in [region_from_texture.py](Tools/Blender/ao_region_tools/region_from_texture.py)) converts a textured mesh into the attributes above, so a textured model doesn't need its regions assigned by hand. It works on a copy, `<object>_Regions`, and hides the original.

**What it does**
1. Averages the color texture down (`analysis_size`) and finds its colors from area-weighted samples of the surface, so texture padding and unused UV space don't count. Hue and saturation are measured independently of darkening, so a darker tone of the same color stays in one region and goes to `Shade`.
2. Subdivides only faces that a boundary can't cross with a single straight cut, or whose fold depth a flat face would miss.
3. Cuts each boundary crossing into the edges at the exact point (bisection in UV), connects the cuts across each face, and labels every face. Islands under `min_region_area` merge into their neighbor.
4. Dissolves refinement vertices that ended up inside one region (`dissolve_unneeded`). A vertex is only dissolved when merging its faces is safe: every edge around it is continuous in every UV map (not a UV seam), its faces share material and smoothing, and no edge is marked seam or sharp. Each pass dissolves vertices that share no edge, so each merge only rebuilds one vertex's ring of faces. Original vertices are never removed; new ones get interpolated bone weights.
5. Writes one **vertex group per region**: `Region1` has the most faces, then `Region2`, and so on. It also writes `RegionGroup` (face int), `Shade`, and `RegionPreview` (the palette color, made the active attribute for the viewport). Region IDs are **not** written by default; you assign them per group afterwards (below).
6. Optionally displaces cloth regions inward along painted folds (`depth_source`).

The `RegionReport` text block lists each group's face count, color and linear palette value. It also gives the color distance between every pair of groups (`color_distances`), which is what `region_merge_distance` compares against, and the `ShadeStrength` that reproduces the texture's darkening (= `shade_range`).

**Assigning region IDs to the groups**
1. Check the groups against `RegionPreview`. Fix any by hand: select faces, then **Assign** them to the group they belong in.
2. Run the script again on the `_Regions` object with `apply_ids`, mapping each group to its region ID on the kit's index plan, e.g. `apply_ids = "Region1:4,Region2:0,Region3:5"` (or `"1:4,2:0,3:5"`). Unlisted groups get region 0 with a warning.
3. This writes `RegionID` (pre-compensated, set as the render attribute) and `RegionIndex`. Run it again whenever the mapping or the groups change.

Prefer `apply_ids` over selecting a group and using **Set Attribute**. Boundary vertices belong to both neighboring groups, so selecting a group in face mode also selects the faces of the other region whose vertices all lie on the boundary (about 2.5% of faces on the test mesh). `apply_ids` resolves those exactly. It also honors your edits by comparing the groups with the membership recorded when they were written (`RegionGroupMask`):
- A face whose vertices you assigned to a group goes to that group.
- A face you removed from its group goes to another group that holds all of its vertices.
- A face whose vertices were already all in the target group can't be detected as moved. Remove it from its old group as well.

**Installing (Blender 4.2+)**
- Edit → Preferences → Get Extensions → ▾ (top right) → **Install from Disk** → `Tools/Blender/dist/ao_region_tools-1.0.7.zip`. Installing a newer version replaces the old one; restart Blender afterwards.
- After changing the source, rebuild the zip: `blender -b --command extension build --source-dir Tools/Blender/ao_region_tools --output-dir Tools/Blender/dist`, then install it again. Bump `version` in `blender_manifest.toml` for a release.
- Blender 3.6 isn't supported (extensions start at 4.2, and the engine has only been tested in 4.2).

**Using the add-on** (3D View → Sidebar `N` → **AO** tab)
1. **Color Regions:** select the textured mesh, optionally pick the texture and UV map (the Texture field stays set when you switch objects; a red warning appears under it when the chosen image isn't one the object's material uses), and adjust the settings (sections Input, Finding regions, Groups and IDs, Geometry, Shade, Depth; hover a setting for its description). Click **Generate Regions**. A status line under the button shows "In progress" while it runs (Blender stops responding until it finishes), then "Done" with the group and triangle counts, or "Failed" with the error. The result `<object>_Regions` becomes the active object, and the full report is in the `RegionReport` text block.
2. **Region IDs:** the table lists each group's color, face count and region ID. The arrow button selects that group's generated faces in Edit Mode (Shift adds to the selection). Edit the groups if needed, set each group's ID, and click **Apply Region IDs**. Giving several groups one ID joins them into one region. **Region Colors / Region IDs** switch the Solid viewport to show `RegionPreview` or the written `RegionID`.
3. The settings are stored in the scene and the ID table on the object, so both are saved with the .blend.

**Presets** (the Preset dropdown above Generate Regions) set the group of settings that defines a kind of surface. They leave the texture, UV map, convention, analysis size, triangle target and method alone, and the panel shows the last one applied.

| Preset | What it sets | Use for |
|---|---|---|
| Cloth with creases | Tones of one color fold into Shade (`lightness_weight` 0.2, `shade_group_distance` 0.035, tolerances 0.1 / 5 / 1 stops, `shade_range` 0.94); luminance depth with strength, maximum and subdivision threshold scaled to the active object's largest dimension (0.77%, 0.82%, 0.012%); 6 color and 2 depth levels | Shirts, trousers, anything with painted folds (the v6 approach) |
| Camo (tones are regions) | `region_count` 3, `lightness_weight` 1.0, shade grouping and all three tolerances off, `min_region_area` 0.00002, no depth | Camo and other patterns whose colors are one hue at different brightness |
| Hard surface (distinct colors) | Automatic regions at `lightness_weight` 0.35, tolerances 0.12 / 4 / 1 stops, no shade grouping, no depth | Gear, helmets with their mixed texture, props |
| Defaults | Every setting back to its default, except the texture and UV map | Starting over |

The tool can't tell on its own whether a brightness difference is shading or a separate color; these settings decide. The cloth preset on a camo texture gives one region with shading (measured on the Voodoo helmet: one region at 99.5%), and the camo preset on cloth turns shading into regions.

New settings added to `Settings` in the engine show up in the panel automatically, under the `# --- Section ---` comment they sit below, with their line comment as the tooltip.

**Running without the add-on**
- In Blender: open `region_from_texture.py` in the Text Editor, edit `SETTINGS = Settings(...)` near the top, select the mesh, Run Script.
- Headless: `blender -b scene.blend --python Tools/Blender/ao_region_tools/region_from_texture.py -- --object_name Body --depth_source luminance --depth_regions "#738040" --save out.blend --report out.json`, then `-- --object_name Body_Regions --apply_ids "1:4,2:0,3:5" --save out.blend`. Every setting is also an option.

**Main settings**
| Setting | Effect |
|---|---|
| `region_count` / `palette` | Fixed count, or fixed sRGB hex colors. `0` = automatic: the fewest regions that put `auto_percentile` of the surface within `merge_distance` of its region's color |
| `merge_distance`, `auto_min_gain` | Automatic mode's threshold. Anti-aliased edge texels set a floor the spread can't go below (about 0.02–0.04). If `merge_distance` is under that floor, it takes the fewest regions within `auto_min_gain` of the best spread. Stricter values split a color's shades into separate regions. `kmeans_restarts` keeps the result stable from run to run |
| `shade_group_distance` | Joins regions that are one color at different brightness: hue and saturation (lightness ignored) within this distance, and no more than `darker_tolerance_stops` below the group's lightest tone. Each texel keeps its nearest color's boundary and then takes that color's group, so the tones become Shade instead of separate regions. Read the report's `shade_range_needed` per region and raise `shade_range` to cover the grouped tones. On the Voodoo torso the greys are 0.003–0.034 apart in hue and skin is 0.046+ away, so 0.035 groups the greys and keeps skin separate. 0 = off |
| `region_merge_distance` | Joins any two regions whose colors are closer than this, in any mode. Read `color_distances` in the report to pick it: a value between two pairs' distances joins the closer pair only |
| `lightness_weight` | How much lightness alone separates regions. Lower folds more tones into `Shade` |
| `color_tolerance`, `min_color_share` | After clustering, any solid color farther than `color_tolerance` from every region gets its own region, however little surface it covers, down to `min_color_share` (default 0.01%). Clustering is driven by area, so without this small multi-color patches and logos fold into the cloth. Anti-aliased edge texels (fewer than 3 of 8 neighbors share their color) don't count, so the blend between two patch colors doesn't become a region. Islands of such a distinct color are also protected from `min_region_area` down to `min_color_share`. 0 = off |
| `darker_tolerance_stops`, `lighter_tolerance_stops` | Within one color, tones more than this many stops darker (default 4) or brighter (default 1) than its light tone get their own region. This separates white on black, which the darkening-invariant color comparison otherwise reads as shaded white. Creases are about 1 stop and baked shadow up to about 3, so lower values split shading into bands. Brighter tones split sooner because Shade can only darken. 0 = off |
| `min_region_area` | Smallest island kept (fraction of surface); smaller islands fold into the neighbor they share the most edge with. Controls stitching/logo detail vs. noise |
| `write_vertex_groups`, `group_prefix` | The per-region vertex groups (`Region1` = most faces) |
| `write_region_ids`, `index_remap` | Write `RegionID` immediately instead of using `apply_ids`, with regions ordered by area (region 0 = largest) and optionally remapped (`"0:4,1:0"`) |
| `apply_ids` | Separate step: group → region ID mapping, written from the groups (see above) |
| `boundary_tolerance_px` | How closely cuts follow a curved boundary before a face is split; lower = more triangles |
| `max_subdiv_levels` | How many times a face may be halved to follow a **color** boundary. Painted letters need many (source faces about 40 texels across need 5–6 levels to reach 1–2-texel strokes) |
| `depth_subdiv_levels` | How many times a face may be halved to follow a **fold**. Kept separate so creases don't take the budget letters need |
| `max_samples_per_edge`, `sample_spacing_px` | How densely each face is checked for boundaries. Below one sample per texel, thin strokes on large faces go unseen and never trigger refinement |
| `min_edge_length` | Faces whose longest edge is shorter (world units) are not split further |
| `max_triangles` | Target for the finished mesh. Generation itself isn't capped (only a 5M safety limit). Afterwards a reduction pass decimates to the target with Blender's collapse decimation, whose cost is the change in shape: flat, uniform areas go first and creases last. 0 = no reduction |
| `reduce_keep_original_vertices`, `reduce_small_region_protection`, `reduce_shade_tolerance`, `reduce_protection_strength` | Steer the reduction through a per-vertex weight. Region borders, open mesh borders, UV seams, marked seam/sharp edges and (by default) the source mesh's own vertices are locked; an edge between two locked vertices is never collapsed, so region borders stay exact. Inside a region the weight is its area share relative to the largest region, to the power `reduce_small_region_protection` (small regions trimmed less), lowered where Shade changes by more than `reduce_shade_tolerance` (linear). `reduce_protection_strength` is how strongly low weights resist (decimate vertex-group factor; 1–100 measured the same on the torso) |
| `shade_range`, `shade_reference_percentile` | Which tone counts as a region's unshaded color, and how much darkening is Shade 1 |
| `depth_source` | `luminance` (painted folds: darker = deeper), `height`, or `normal` (integrated tangent-space normal map; `normal_green` = `directx` for UE maps) |
| `depth_regions`, `depth_strength`, `depth_max`, `depth_curve` | Which regions move: their sRGB colors from the report (`"#738040"`, matched to the nearest region), since group numbers are only known after the pass. Then how far (world units), and how strongly shallow folds count |
| `depth_blur_px`, `depth_edge_falloff_px`, `depth_smooth_iterations`, `depth_subdiv_threshold` | Fold smoothness, fade at hems, and how much density depth may add |
| `depth_preserve_volume`, `depth_volume_radius_px` | Luminance folds only point inward, and most texels are a little darker than their region's light tone, so on its own depth shrinks the whole surface. This subtracts the local average depth (within the radius, about the spacing between creases): creases go in and the cloth between them comes out, so the volume stays. 1 = full, 0 = off |
| `depth_max_thickness` | Inward depth is capped at this fraction of the part's thickness, measured by a ray straight through the part along the inward normal. Protects fingers, straps and flaps. 1 = off |
| `depth_threshold` | Fold amounts below this stay flat; the rest is rescaled to 0–1. Use it if dirt or mottling dents the surface |

**Known limits**
- The result is triangulated. UVs are kept and interpolated. Before 1.0.1, `dissolve_unneeded` merged faces across UV seams, so some faces' UVs spanned two islands (a doubled-looking texture in the textured view); 1.0.1 keeps those vertices. Measured on the Voodoo torso without depth: 2,761 island-spanning faces before, 16 after, 5 with dissolve off, at 142k vs 508k triangles.
- Measured on the Voodoo torso with the v4 settings: depth alone shrank the torso's volume by 6.6% and thinned parts under 3 units thick by 16% of their thickness per side. With `depth_preserve_volume` 1 the volume change is −0.1%, those parts lose 3%, and creases sit about twice as deep relative to the cloth around them. Reproduce older results with `depth_preserve_volume` 0 and `depth_max_thickness` 1.
- Color tolerances, measured on the Voodoo torso with v4 settings: the defaults add red and navy regions for the NRA patch (about 0.04% of the surface each) and a near-black region (7%, likely the black straps and buckles), at 209k vs 140k triangles. The patch's white stays with the light fabric (within 1 stop); `lighter_tolerance_stops` 0.5 separates it but also splits fabric highlights into extra regions. On a synthetic test (grey cloth with creases, white stripes on black, a small red/white/blue/green patch), the defaults found all six colors; with tolerances off, only grey and black.
- Reduction, measured on the Voodoo torso (v5 settings with 6 color / 3 depth levels, 4096 analysis, shade grouping 0.035): generation made 1.04M triangles. Reducing to 600k and to 300k both kept patch agreement at 87.0% and the red/navy letter IoU at 0.78/0.76, and crease relief (99th percentile distance from the source surface) at 0.229/0.225 vs 0.232 unreduced. At 300k the patch kept 70% of its faces while the mesh kept 29%. Decimation takes about 10 s. Locked original vertices show as triangle fans in flat areas.
- Depth displacement can still fold triangles in deep, narrow creases (about 650 island-spanning or folded faces on the Voodoo torso with v2 depth settings, with or without dissolve).
- A boundary crossing a UV seam is placed separately on each side, so the two cut points can differ slightly.
- `luminance` depth can't tell a fold from a darker fabric. Restrict it with `depth_regions`, or use a normal or height map when one exists.
- Region and depth refinement add triangles to skinned meshes (non-Nanite); check the report against the polycount budget (Engine.md, test E).
- `decimate_ratio < 1` only adds an unapplied Decimate modifier that excludes boundary vertices; it has not been tuned.
- UDIM textures aren't supported (UVs wrap into one tile).

**Verified (2026-11-08, Blender 4.2):** on a rigged test sphere it found all four painted regions and merged the speck. Groups were numbered by face count, every face's vertices were in its own group, and `apply_ids` wrote the mapped IDs, including hand-assigned faces. `region_merge_distance` set between the two closest pairs joined only the closest. 99.9% of faces matched the texture, every new vertex's weights summed to 1, the largest weight error was 0.015, and the shaded render matched the textured original. On Manny LOD0 with a stand-in texture: 92k → 112k triangles in 15 s, with fold depth on three regions. It hasn't been run on a real textured character yet.

### Targeted detail (prototype, `method` = Targeted)

Instead of refining the whole mesh, targeted mode keeps the base mesh as it is and injects detail islands only where the texture needs them ([targeted.py](Tools/Blender/ao_region_tools/targeted.py)):

1. **Prepass, color borders:** base faces a color boundary crosses, checked about once per texel.
2. **Prepass, height:** base faces whose flat surface misses the fold depth by more than `depth_subdiv_threshold`.
3. **Islands:** connected detail faces within one UV island. `targeted_area` = Selection uses the source mesh's selected faces instead.
4. **Injection, per island:** color boundaries are traced in the region map and simplified to `targeted_tolerance_px`. The island is rebuilt with a constrained Delaunay triangulation (Blender's `delaunay_2d_cdt`), with the boundaries and the base edges as constraints, so every new triangle lies inside one region and one base triangle and the surface keeps its shape. Height points are then added, up to `targeted_depth_iterations` rounds, wherever folds are still missed. New vertices take position, bone weights and UVs from the base triangle they lie in. Island borders keep their edges; a new point on one splits the outside face's edge too.
5. The rest of the pipeline is unchanged. Depth only moves island interiors, and the reduction pass runs only if the result is above `max_triangles`.

| Setting | Effect |
|---|---|
| `method` | Subdivide (refine the whole mesh) or Targeted |
| `targeted_area` | Automatic (color-border faces, plus fold faces per the depth scope) or Selection |
| `targeted_depth_scope` | Height detail Everywhere (fold faces become islands too), only in Color areas, or None |
| `targeted_tolerance_px` | How closely island edges follow a painted boundary (analysis texels); higher = fewer triangles |
| `targeted_depth_iterations`, `targeted_min_spacing_px` | Rounds of height points, and their minimum spacing (texels) |
| `targeted_keep_base_edges` | Keep the base edges inside islands, so new triangles never cut across a base fold |

**Measured (2026-11-09, Voodoo torso, v6 settings):**

| | Triangles | Patch agreement | Red letters IoU | Time |
|---|---|---|---|---|
| Subdivide | 298,500 (after reduction) | 91.2% | 0.64 | 121 s |
| Targeted, your 15 selected faces | 16,346 (whole torso) | 90.9% | 0.63 | 22 s |
| Targeted, automatic, height in color areas | 260,612 | 87.7% | 0.63 | 87 s |
| Targeted, automatic, height everywhere | 843k → 298,500 after reduction | 88.0% | 0.63 | 493 s |

On a synthetic rigged sphere: 4,278 vs 14,166 triangles for subdivide, 98.8% of faces matching the texture, no open or non-manifold edges, and bone weights intact.

**Prototype limits:**
- Automatic islands on the torso still leave a few cracks along island borders: 16 more open edges than the base mesh with height in color areas, and 47 plus 5 non-manifold with height everywhere. Selection islands came out clean.
- Kept base edges make thin sliver fans between traced boundaries and the base edges. That's fine for flat color but can spike under depth; a triangle-quality pass would fix it.
- Outside islands the base mesh isn't displaced, so creases outside islands have no geometry (use height scope Everywhere).
- `depth_subdiv_threshold` 0.015 makes height-everywhere insert hundreds of thousands of points on the torso. Use a coarser threshold in targeted mode.
- An island never spans a UV seam (it stops there), and other face-corner attributes on the source (extra color layers) aren't carried into islands.

### Pass-by-pass version (AO Color Region Passes, separate add-on)

[Tools/Blender/ao_region_passes](Tools/Blender/ao_region_passes) (`dist/ao_region_passes-0.1.3.zip`) is the same tool split into passes, each with its own button, so a setting can be changed and only its pass re-run. It is a separate extension, with its own sidebar tab (**AO Passes**), operators, stored settings and engine copy, so it installs alongside AO Color Regions without affecting it.

**How the mesh is made.** Every run turns the texture into three maps, and builds the geometry from them. Color settings never place vertices: they decide the region map, and geometry can only follow boundaries that exist in it.

| Pass | Reads | Makes | Places vertices? |
|---|---|---|---|
| 1. Analyze | The texture; Regions, Small and separate colors, Shading, Fold map settings | Region map (each texel's region, in its color), shade map (how much darker than its region's light tone), fold map (depth per texel), saved as images in the .blend (`AOP_<source>_regions` / `_shade` / `_fold`) | No |
| 2. Geometry | The region map, and the fold map for fold detail | `<source>_Passes`, flat: faces split where a boundary or fold needs them (Subdivide) or rebuilt along traced boundaries (Targeted), then cut exactly along every boundary | **Yes, the only pass that does** |
| 3. Depth | The fold map; Depth strength, max, thickness, smoothing | Moves `<source>_Passes`'s vertices along their normals. Always starts from the flat positions Geometry saved, so a new strength replaces the old one | No (moves only) |
| 4. Reduce | Max triangles and the reduce settings | `<source>_Passes_Reduced`, decimated from `<source>_Passes` (flat, uniform areas first; borders kept) | No (removes only) |
| 5. Region IDs | | As in the original add-on, on whichever result is active | |

**Iterating:**
- Each pass panel shows **Done**, **Not run** or **Out of date**, with the reason: which of its settings changed, or that an earlier pass was re-run.
- **Regions / Shade / Folds** (under Analyze) open a map in an Image Editor. Use them to check what counts as a color and what as height before building geometry.
- **Painting on the region map** fixes a region by hand. Paint with a region's exact color (eyedropper from the map), then re-run Geometry. In the test, a painted block became that region on all 9 faces it covered. Use Image > Pack afterwards to keep the edit in the file.
- **Flatten** (under Depth) removes depth without rebuilding.
- **Keeping a result (0.1.3):** rename it. Each source points to its current Geometry and Reduce results, and the pointer follows renames, so Depth and Reduce keep working on the result you renamed. The next Geometry run builds a new `<source>_Passes` and leaves the renamed one alone. Only a result that still has the standard name is replaced. A duplicate (Shift+D) is a copy the passes never touch; the main panel warns when one is selected and names the result the passes use. Files from 0.1.2 and earlier have no pointer yet: the newest Geometry result counts as current until the next run sets it.
- Depth's fold detail is placed by Geometry using the Depth strength at that time; the Depth panel says so when the strength has changed a lot since.

**Where vertices around colors come from**, in order of effect: the color must exist in the region map (check with Regions); `analysis_size` at the texture's full size; `majority_iterations` 1 or `majority_radius` 0 so thin strokes survive; a small `min_split_fraction` (about 0.02) so cuts don't snap to nearby vertices; more color levels and a smaller `boundary_tolerance_px` (Subdivide) or a smaller `targeted_tolerance_px` (Targeted).

**Shade edges: mesh edges inside a region (0.1.1).** Region borders alone decide where faces are cut. Once `shade_group_distance` folds tones of one color into one region, their outlines (blobs, dirt, creases) are no longer cut, and only reach the mesh as Shade interpolated across large faces. `shade_edge_stops` (Geometry > Vertices along shade edges) adds those outlines back without adding regions. Geometry also cuts wherever a region's shade crosses another step of that many stops of darkening.
- Each face records its step in the `ShadeStep` face attribute. Its corners' Shade is kept within that step, so the change at a step's edge is crisp.
- Region count and IDs are unchanged.
- `shade_edge_blur_px` smooths the shade map within each region before it is cut into steps. `shade_edge_cleanup_px` drops patches narrower than about twice its value (specks, noise).
- **Preview Shade Edges** (in the same section) saves `AOP_<source>_shade_edges` and shows it in the Image Editor without building a mesh. It shows each region's color darkened to the middle of each step, the flat tones the mesh will have between its edges. It also reports the number of edge texels, which can be compared between settings.
- Reduce protects the edges between steps without locking them (`reduce_shade_edge_weight`, 0.05). It can straighten jagged outlines, so the mesh can still reach Max triangles; 0 locks them like region borders.
- Steps only exist within `shade_range` (Analyze > Shading): everything darker than that clips to Shade 1, the last step. At 0.6 that is 1.3 stops, so 0.5-stop edges give only 3 steps; 0.94 allows about 4 stops.
- `max_subdiv_levels` (Subdivide) and `targeted_tolerance_px` (Targeted) set how closely shade edges are followed, as for color edges.
- Tested (2026-11-13, Voodoo torso, 3 regions from `shade_group_distance` 2.0, `shade_range` 0.6, 4096 maps):
  - 0.5-stop edges give 1.96M edge texels, against 762k for region borders alone.
  - Targeted: 729k triangles in 6–7 min, with 295k step edges. Subdivide at 4 levels: 2.56M triangles in 3.5 min.
  - Reduce to 300k reached 298.5k and kept 131k step edges. With the edges locked (weight 0) it stopped at 647k.
  - No Shade corner fell outside its face's step.

**Height jitter** comes from luminance depth reading every darkening (dirt, mottling, weave) as depth, and from volume preservation pushing everything between dark spots outward. Use `depth_threshold` 0.15–0.25, `depth_curve` 1.5–2, `depth_blur_px` 2–3, and `depth_regions` set to the fabric colors. Check the result in the fold map before building geometry.

**Tested (2026-11-13, Blender 4.2), 21 checks:** both add-ons registered together; Analyze changes no mesh and saves three packed maps; Geometry is flat (vertices on the source surface); Depth at strength 0.06 moves vertices twice as far as at 0.03 (replaces, doesn't stack), and Flatten restores the flat mesh; out-of-date detection for settings and for re-run earlier passes; painting on the region map; Reduce keeps bone weights; Region IDs on the reduced result; the original add-on still works; the maps survive save and reopen. `Tools/Blender/restore_v6_settings.py` restores v6 in either add-on.

## Shade channel (main + shaded tones)

Use this to give one palette color lighter and darker areas without spending extra palette slots.

- Create an attribute named `Shade` (Color). Use the **Face Corner** domain for hard two-tone bands, or **Point** for soft gradients.
- Paint a **darkening amount**: 0 = no darkening, 1 = maximum. A new attribute is black (0) = unshaded, so no setup is needed.
  - Hard bands: Edit Mode → select faces → **Mesh → Set Attribute** with a gray value.
  - Soft gradients: **Vertex Paint** mode with a grayscale brush (make `Shade` the active attribute first).
- Material: `BaseColor = palette[region] × (1 − B × ShadeStrength)`.
- Gamma affects nonzero values, so judge the result in UE rather than in Blender's viewport.

## AO channel

- Create an attribute named `AO` (Color, **Point** domain) and bake AO into it with Cycles (Bake → Output target: **Color Attributes**).
- Needs enough vertex density in creases for a smooth result. On skeletal meshes it's baked in the bind pose.

## Packing for export

UE imports one vertex color set, so the separate attributes are combined into one just before export.

**With the passes add-on (0.1.2):** select the Geometry or Reduce result and click **Pack for Export** (5. Region IDs). It makes `<mesh>_Export` holding only `VertexColorPacked` (R = RegionID, G = 1, B = Shade, A = 1), selects it, and leaves the result unchanged. It refuses when RegionID is missing or the mesh is written for a different **Import in UE as** type than the setting. Export that copy: File > Export > FBX, Limit to Selected Objects (plus the armature for a skeletal mesh), Geometry > Vertex Colors = **Linear**. `Tools/Blender/pack_for_export.py` does the same from the Text Editor for meshes outside the add-on.

**By hand:**

1. On an **export copy** of the mesh, add a Geometry Nodes modifier.
2. Read each attribute with **Named Attribute** (Color): `RegionID`, `AO`, `Shade`.
3. **Combine Color**: R = RegionID's R, G = AO, B = Shade, A = spare (1.0 or a mask).
4. **Store Named Attribute** → Face Corner, Color, name `VertexColorPacked`.
5. Apply the modifier, then make `VertexColorPacked` the **render** attribute (camera icon).

Keep the original attributes on the source mesh so they stay editable.

## Animated parts (WPO)

For moving parts on static meshes (e.g. a weapon's bolt), animated in the material. See [Rendering.md](Rendering.md) → "Material animation (World Position Offset)".

- The moving part needs to be identifiable in the material: give it its **own region** (if it's a single color), or a mask in a spare channel (A).
- Model the part in its **rest position**. The material offsets it from there.
- Note the movement direction in the mesh's **local** axes; the material offsets along that axis.
- Keep the movement small enough to stay near the mesh bounds, or note the maximum distance so the material's *Max World Position Offset Displacement* can be set.

## Export / import checklist

- [ ] Export from the packed copy, with `VertexColorPacked` as the render attribute (see "Packing for export"). If the mesh only uses regions, `RegionID` alone as the render attribute is fine.
- [ ] FBX export: **Geometry → Vertex Colors = Linear**.
- [ ] UE import: **Vertex Color Import Option = Replace**.
- [ ] UE import: **Material Import Method = Do Not Create Material**, then assign the master material instance.
- [ ] Skeletal meshes: import onto the existing skeleton so animations still apply.
- [ ] Verify with the debug-color material (hard-coded switch colors) before assigning the real palette.
- [ ] **Material usage flags** (material → Details → Usage), saved in the asset: **Used with Nanite** for Nanite meshes, **Used with Instanced Static Meshes** for foliage/ISM/PCG, **Used with Skeletal Mesh** for characters. The editor sets these automatically, so things look correct in the editor, but a **packaged build** only has the flags that were saved. A missing flag makes the cooked game substitute the default material, and the mesh can end up rendered through the traditional (non-Nanite) path (found 2026-10-25, which skewed the first Nanite profiling runs). Check the packaged log for `missing usage flag` after every cook.

## Channel layout (packed export attribute)

| Channel | Source attribute | Use | Gamma note |
|---|---|---|---|
| R | `RegionID` | Region ID | pre-compensate (tables above) |
| G | `AO` | Baked AO | gamma-converted; bake visually, check result in UE |
| B | `Shade` | Darkening amount (main + shaded) | gamma-converted for nonzero values; check in UE |
| A | — | Spare: WPO mask, or test whether alpha arrives *without* gamma conversion (if so, it's the cleaner home for Region ID) | test first |

## Palette values (Custom Primitive Data)

- Floats are **0–1** and **linear**. A color copied from an sRGB source (hex code, Photoshop) needs converting first (≈ `value^2.2`), or pick it with UE's color picker and copy the linear values it shows.
- Region `k`'s color lives at Custom Primitive Data index `k × 4` (RGBA).
