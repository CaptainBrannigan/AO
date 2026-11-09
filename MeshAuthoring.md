# Mesh Authoring Reminders

Checklist for authoring meshes that use the textureless color-region material (see [Rendering.md](Rendering.md) → "Color regions").

> ## ⚠ IMPORTANT: region IDs must be pre-compensated for gamma
>
> Vertex color values are gamma-converted (sRGB → linear) between Blender and the shader. A value typed as `0.2` arrives in the material as roughly `0.033`. If you type plain step values (0.2, 0.4…), every region decodes **one index too low**.
>
> **Always type the pre-compensated values from the tables below**, not the plain step values. The material then decodes with no Power node.
>
> If FBX export settings or the attribute type ever change, re-run the debug-color test before trusting these values.

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
- Edit → Preferences → Get Extensions → ▾ (top right) → **Install from Disk** → `Tools/Blender/dist/ao_region_tools-1.0.3.zip`. Installing a newer version replaces the old one; restart Blender afterwards.
- After changing the source, rebuild the zip: `blender -b --command extension build --source-dir Tools/Blender/ao_region_tools --output-dir Tools/Blender/dist`, then install it again. Bump `version` in `blender_manifest.toml` for a release.
- Blender 3.6 isn't supported (extensions start at 4.2, and the engine has only been tested in 4.2).

**Using the add-on** (3D View → Sidebar `N` → **AO** tab)
1. **Color Regions:** select the textured mesh, optionally pick the texture and UV map, and adjust the settings (sections Input, Finding regions, Groups and IDs, Geometry, Shade, Depth; hover a setting for its description). Click **Generate Regions**. A status line under the button shows "In progress" while it runs (Blender stops responding until it finishes), then "Done" with the group and triangle counts, or "Failed" with the error. The result `<object>_Regions` becomes the active object, and the full report is in the `RegionReport` text block.
2. **Region IDs:** the table lists each group's color, face count and region ID. The arrow button selects that group's generated faces in Edit Mode (Shift adds to the selection). Edit the groups if needed, set each group's ID, and click **Apply Region IDs**. Giving several groups one ID joins them into one region. **Region Colors / Region IDs** switch the Solid viewport to show `RegionPreview` or the written `RegionID`.
3. The settings are stored in the scene and the ID table on the object, so both are saved with the .blend.

New settings added to `Settings` in the engine show up in the panel automatically, under the `# --- Section ---` comment they sit below, with their line comment as the tooltip.

**Running without the add-on**
- In Blender: open `region_from_texture.py` in the Text Editor, edit `SETTINGS = Settings(...)` near the top, select the mesh, Run Script.
- Headless: `blender -b scene.blend --python Tools/Blender/ao_region_tools/region_from_texture.py -- --object_name Body --depth_source luminance --depth_regions "#738040" --save out.blend --report out.json`, then `-- --object_name Body_Regions --apply_ids "1:4,2:0,3:5" --save out.blend`. Every setting is also an option.

**Main settings**
| Setting | Effect |
|---|---|
| `region_count` / `palette` | Fixed count, or fixed sRGB hex colors. `0` = automatic: the fewest regions that put `auto_percentile` of the surface within `merge_distance` of its region's color |
| `merge_distance`, `auto_min_gain` | Automatic mode's threshold. Anti-aliased edge texels set a floor the spread can't go below (about 0.02–0.04). If `merge_distance` is under that floor, it takes the fewest regions within `auto_min_gain` of the best spread. Stricter values split a color's shades into separate regions. `kmeans_restarts` keeps the result stable from run to run |
| `region_merge_distance` | Joins any two regions whose colors are closer than this, in any mode. Read `color_distances` in the report to pick it: a value between two pairs' distances joins the closer pair only |
| `lightness_weight` | How much lightness alone separates regions. Lower folds more tones into `Shade` |
| `color_tolerance`, `min_color_share` | After clustering, any solid color farther than `color_tolerance` from every region gets its own region, however little surface it covers, down to `min_color_share` (default 0.01%). Clustering is driven by area, so without this small multi-color patches and logos fold into the cloth. Anti-aliased edge texels (fewer than 3 of 8 neighbors share their color) don't count, so the blend between two patch colors doesn't become a region. Islands of such a distinct color are also protected from `min_region_area` down to `min_color_share`. 0 = off |
| `darker_tolerance_stops`, `lighter_tolerance_stops` | Within one color, tones more than this many stops darker (default 4) or brighter (default 1) than its light tone get their own region. This separates white on black, which the darkening-invariant color comparison otherwise reads as shaded white. Creases are about 1 stop and baked shadow up to about 3, so lower values split shading into bands. Brighter tones split sooner because Shade can only darken. 0 = off |
| `min_region_area` | Smallest island kept (fraction of surface); smaller islands fold into the neighbor they share the most edge with. Controls stitching/logo detail vs. noise |
| `write_vertex_groups`, `group_prefix` | The per-region vertex groups (`Region1` = most faces) |
| `write_region_ids`, `index_remap` | Write `RegionID` immediately instead of using `apply_ids`, with regions ordered by area (region 0 = largest) and optionally remapped (`"0:4,1:0"`) |
| `apply_ids` | Separate step: group → region ID mapping, written from the groups (see above) |
| `boundary_tolerance_px` | How closely cuts follow a curved boundary before a face is split; lower = more triangles |
| `min_edge_length`, `max_subdiv_levels`, `max_triangles` | Refinement limits (world units) |
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
- Depth displacement can still fold triangles in deep, narrow creases (about 650 island-spanning or folded faces on the Voodoo torso with v2 depth settings, with or without dissolve).
- A boundary crossing a UV seam is placed separately on each side, so the two cut points can differ slightly.
- `luminance` depth can't tell a fold from a darker fabric. Restrict it with `depth_regions`, or use a normal or height map when one exists.
- Region and depth refinement add triangles to skinned meshes (non-Nanite); check the report against the polycount budget (Engine.md, test E).
- `decimate_ratio < 1` only adds an unapplied Decimate modifier that excludes boundary vertices; it has not been tuned.
- UDIM textures aren't supported (UVs wrap into one tile).

**Verified (2026-11-08, Blender 4.2):** on a rigged test sphere it found all four painted regions and merged the speck. Groups were numbered by face count, every face's vertices were in its own group, and `apply_ids` wrote the mapped IDs, including hand-assigned faces. `region_merge_distance` set between the two closest pairs joined only the closest. 99.9% of faces matched the texture, every new vertex's weights summed to 1, the largest weight error was 0.015, and the shaded render matched the textured original. On Manny LOD0 with a stand-in texture: 92k → 112k triangles in 15 s, with fold depth on three regions. It hasn't been run on a real textured character yet.

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

UE imports one vertex color set, so the separate attributes are combined into one just before export:

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
