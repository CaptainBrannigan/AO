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
