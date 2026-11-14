"""
Region from texture: turns a textured mesh's color texture into AO color-region mesh data.

Region pass, on a copy of the mesh:
  Region1..N     vertex groups, one per region, Region1 = most faces (boundary vertices are in both neighbors)
  RegionGroup    face int, the group number of each face
  Shade          face-corner color, darkening amount (main + shaded), optional
  RegionPreview  face-corner color, the region's palette color, for checking in the viewport
and optionally displaces cloth regions to give folds real depth.

ID step (apply_ids = "1:4,2:0,..."), run on the processed object after checking/editing the groups:
  RegionID       face-corner color, region ID in R (gamma pre-compensated), see MeshAuthoring.md
  RegionIndex    face int, the plain region ID

Region boundaries become real edges: faces are subdivided only where a boundary or a fold needs it,
boundary crossings are cut into the edges, and refinement vertices that turned out unnecessary are
dissolved again. Original vertices are never moved (except by depth) or removed, so skin weights stay intact;
new vertices get interpolated weights.

Run inside Blender: open in the Text Editor, edit SETTINGS below, select the mesh, Run Script.
Run headless:
  blender -b scene.blend --python region_from_texture.py -- --object_name Body --region_count 6 --save out.blend
  blender -b out.blend --python region_from_texture.py -- --object_name Body_Regions --apply_ids "1:4,2:0,3:5" --save out.blend
Every Settings field is also a command-line option (--name value).
"""

import bpy
import bmesh
import mathutils
import mathutils.kdtree
import mathutils.bvhtree
import math
import sys
import json
import time
import argparse
from dataclasses import dataclass, fields, asdict

import numpy as np

try:
    from . import targeted as _targeted
except ImportError:                     # run as a plain script: the module sits next to this file
    import os as _os
    sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
    import targeted as _targeted


@dataclass
class Settings:
    # --- Input ---
    object_name: str = ""              # empty = active object
    image_name: str = ""               # color texture; empty = image feeding the material's Base Color
    uv_layer: str = ""                 # empty = active UV map
    output_suffix: str = "_Regions"    # the result is a copy named <object><suffix>
    in_place: bool = False             # modify the object itself instead of a copy
    hide_source: bool = True           # hide the original after making the copy

    # --- Finding regions ---
    region_count: int = 0              # 0 = automatic (smallest count where every texel is within merge_distance)
    palette: str = ""                  # optional fixed sRGB hex colors "#aabbcc,#112233"; overrides region_count
    merge_distance: float = 0.06       # automatic mode: how far (color feature distance) a texel may be from its region's color
    auto_percentile: float = 99.5      # automatic mode: share of the surface that must meet merge_distance (higher keeps small regions)
    auto_min_gain: float = 0.15        # automatic mode, when merge_distance can't be met: fewest regions within this fraction of the best spread
    lightness_weight: float = 0.35     # 0..1: how much lightness separates regions; hue/saturation are measured independent of darkening,
                                       # so a darker tone of the same color differs only by lightness and folds into Shade
    analysis_size: int = 1024          # texture is averaged down to this many texels on its long side
    cluster_samples: int = 200000      # area-weighted surface samples used to find the colors
    kmeans_iterations: int = 30
    kmeans_restarts: int = 4           # independent tries per region count; the best fit is kept (stabler counts and colors)
    seed: int = 1
    majority_radius: int = 1           # texel radius of the cleanup filter on the region map (0 = off)
    majority_iterations: int = 2
    min_region_area: float = 0.0005    # mesh islands smaller than this fraction of total area merge into their neighbor
    region_merge_distance: float = 0.0 # regions whose colors are closer than this (color feature distance) become one; 0 = off
    shade_group_distance: float = 0.0  # regions of one hue/saturation (lightness ignored) within this become one region, their tones going to Shade; limited by darker_tolerance_stops; 0 = off
    convention: str = "0.2"            # "0.2" (up to 6 regions) or "0.125" (up to 9), see MeshAuthoring.md

    # --- Color tolerances ---
    color_tolerance: float = 0.12      # a solid color farther than this from every region gets its own region, however small (patches, logos); 0 = off
    min_color_share: float = 0.0001    # smallest share of the surface such a color (or tone) needs to get its own region
    darker_tolerance_stops: float = 4.0   # within one color, tones more than this many stops darker than its light tone get their own region (white on black ~5-7 stops, creases ~1, baked shadow up to ~3); 0 = off
    lighter_tolerance_stops: float = 1.0  # tones brighter than the light tone by more than this get their own region (Shade can only darken); 0 = off

    # --- Groups and IDs ---
    write_vertex_groups: bool = True   # one vertex group per region: <group_prefix>1 has the most faces, then 2, ...
    group_prefix: str = "Region"
    write_preview: bool = True         # RegionPreview color attribute (each region's palette color) for checking in the viewport
    write_region_ids: bool = False     # also write RegionID/RegionIndex now; off = assign IDs per group afterwards with apply_ids
    index_remap: str = ""              # write_region_ids only: "from:to,..." onto your kit's index plan (default order: largest area first)
    apply_ids: str = ""                # separate step on an already processed object: "1:0,2:4,3:5" (group:region ID) writes RegionID

    # --- Targeted detail ---
    method: str = "subdivide"          # "subdivide": refine the whole mesh; "targeted": keep the base mesh and inject detail islands only where needed
    targeted_area: str = "auto"        # "auto": base faces crossed by color borders (plus fold faces, per targeted_depth_scope); "selection": the source mesh's selected faces
    targeted_depth_scope: str = "everywhere"  # height detail: "everywhere" (fold faces become islands too), "color_areas" (only inside color islands), "none"
    targeted_tolerance_px: float = 1.0 # how closely island edges follow a painted boundary, in analysis texels (higher = fewer triangles)
    targeted_depth_iterations: int = 6 # rounds of adding height points where folds are still missed by more than depth_subdiv_threshold
    targeted_min_spacing_px: float = 2.0  # closest two height points may be, in analysis texels
    targeted_keep_base_edges: bool = True # keep the base mesh's edges inside islands, so the surface shape stays exact

    # --- Geometry ---
    max_subdiv_levels: int = 6         # how many times a face may be split in half to follow a color boundary (painted letters need many)
    min_edge_length: float = 0.004     # world units: faces whose longest edge is shorter are not subdivided further
    boundary_tolerance_px: float = 0.75  # allowed average deviation of a cut from the true boundary, in analysis texels
    sample_spacing_px: float = 1.0     # sample spacing inside faces and along edges, in analysis texels
    max_samples_per_edge: int = 64     # cap on samples per face edge; below one per texel, thin strokes on large faces go unseen
    boundary_search_steps: int = 8     # bisection steps placing each cut on an edge
    min_split_fraction: float = 0.04   # cuts closer than this to an edge's end snap to the existing vertex
    seam_inset: float = 0.02           # samples are pulled this far toward the face center so UV seams don't read the neighbor island
    dissolve_unneeded: bool = True     # remove refinement vertices that ended up inside a single region
    max_triangles: int = 600000        # target for the finished mesh: a reduction pass merges flat faces inside regions until it fits; 0 = no reduction
    reduce_protection_strength: float = 10.0  # how strongly low weights (small regions, Shade detail, border neighbours) resist the reduction (decimate vertex-group factor)
    reduce_small_region_protection: float = 0.5  # 0..1: smaller regions get a proportionally smaller angle (0 = same for all, 1 = scaled by area share)
    reduce_shade_tolerance: float = 0.15   # linear Shade change between neighboring vertices at which the reduction weight halves (painted shading detail)
    reduce_keep_original_vertices: bool = True  # never remove the source mesh's own vertices (skinning)
    decimate_ratio: float = 1.0        # < 1 adds an (unapplied) Decimate modifier that spares region boundaries

    # --- Shade (B channel) ---
    write_shade: bool = True
    shade_range: float = 0.6           # luminance drop (fraction of the region's light tone) that maps to Shade 1; use as the material's ShadeStrength
    shade_reference_percentile: float = 85.0  # which brightness counts as a region's unshaded tone
    precompensate: bool = True         # write sRGB-encoded values so the shader receives the intended linear value

    # --- Depth (cloth folds) ---
    depth_source: str = "none"         # none | luminance | height | normal
    depth_image: str = ""              # height or normal image name; empty = normal map found in the material
    depth_regions: str = ""            # regions to displace: sRGB colors "#738040,#334455" (matched to the nearest region) or region indices; empty = all
    depth_strength: float = 0.01       # world units at full fold
    depth_curve: float = 1.0           # exponent applied to the fold amount (>1 keeps only deep folds)
    depth_max: float = 0.02            # world units clamp
    depth_midlevel: float = 0.5        # height source: value that means no displacement
    depth_blur_px: int = 2             # texel radius blur of the fold map
    depth_edge_falloff_px: int = 6     # folds fade out over this many texels inside a displaced region's edge (hems, seams)
    depth_threshold: float = 0.0       # fold amounts below this stay flat (dirt, mottling); the rest is rescaled to 0..1
    depth_preserve_volume: float = 1.0 # 0..1: subtract this share of the local average depth, so creases go in and the cloth between comes out (no net shrink)
    depth_volume_radius_px: int = 16   # texel radius of that local average (about the spacing between creases)
    depth_max_thickness: float = 0.2   # inward depth is limited to this fraction of the part's thickness (fingers, straps, flaps); 1 = off
    depth_highpass_px: int = 48        # normal source: removes large-scale drift from integration
    depth_smooth_iterations: int = 2   # mesh-space smoothing of the displacement
    depth_subdiv_threshold: float = 0.0015  # world units: refine where flat faces miss the fold by more than this
    depth_subdiv_levels: int = 3       # how many times a face may be split in half to follow a fold (separate from color boundaries)
    normal_green: str = "directx"      # normal source: "directx" (UE) or "opengl" (Blender)


SETTINGS = Settings()
DEBUG_KEEP_LAYERS = False               # tests: keep internal layers (e.g. _rft_island) on the result mesh


# ----------------------------------------------------------------------------------------------
# Logging

_T0 = time.time()


def log(*args):
    print("[regions %6.1fs]" % (time.time() - _T0), *args, flush=True)


# ----------------------------------------------------------------------------------------------
# Color

def srgb_to_linear(c):
    c = np.asarray(c, dtype=np.float32)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4).astype(np.float32)


def linear_to_srgb(c):
    c = np.clip(np.asarray(c, dtype=np.float64), 0.0, None)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1.0 / 2.4) - 0.055)


_M1 = np.array([[0.4122214708, 0.5363325363, 0.0514459929],
                [0.2119034982, 0.6806995451, 0.1073969566],
                [0.0883024619, 0.2817188376, 0.6299787005]])
_M2 = np.array([[0.2104542553, 0.7936177850, -0.0040720468],
                [1.9779984951, -2.4285922050, 0.4505937099],
                [0.0259040371, 0.7827717662, -0.8086757660]])


def linear_to_oklab(rgb):
    lms = np.cbrt(np.maximum(rgb @ _M1.T, 0.0))
    return lms @ _M2.T


def color_features(lin, lightness_weight):
    """OKLab lightness plus a/L, b/L. Darkening a linear color scales L, a and b by the same factor,
    so a/L and b/L stay fixed and a shaded tone of a color differs from it only in lightness."""
    lab = linear_to_oklab(lin)
    L = lab[..., :1]
    ab = lab[..., 1:] / np.maximum(L, 0.15) * 0.6
    return np.concatenate([L * lightness_weight, ab], axis=-1).astype(np.float32)


def luminance(rgb):
    return rgb @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)


def hex_to_linear(h):
    h = h.strip().lstrip("#")
    return srgb_to_linear(np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]))


def linear_to_hex(rgb):
    s = np.clip(linear_to_srgb(rgb), 0, 1)
    return "#" + "".join("%02x" % int(round(x * 255)) for x in s)


def region_value_table(convention):
    step = {"0.2": 0.2, "0.125": 0.125}[convention]
    count = int(round(1.0 / step)) + 1
    return [float(linear_to_srgb(i * step)) for i in range(count)]


# ----------------------------------------------------------------------------------------------
# Texture maps

class Map:
    """A texture-space array sampled by UV (wrapping). Row 0 is v = 0, as in Blender's image.pixels."""

    def __init__(self, a):
        self.a = a
        self.h, self.w = a.shape[:2]

    def nearest(self, uv):
        u = np.mod(uv[..., 0], 1.0)
        v = np.mod(uv[..., 1], 1.0)
        x = np.minimum((u * self.w).astype(np.int32), self.w - 1)
        y = np.minimum((v * self.h).astype(np.int32), self.h - 1)
        return self.a[y, x]

    def bilinear(self, uv):
        u = np.mod(uv[..., 0], 1.0) * self.w - 0.5
        v = np.mod(uv[..., 1], 1.0) * self.h - 0.5
        x0 = np.floor(u).astype(np.int32)
        y0 = np.floor(v).astype(np.int32)
        fx = (u - x0)[..., None] if self.a.ndim == 3 else (u - x0)
        fy = (v - y0)[..., None] if self.a.ndim == 3 else (v - y0)
        x0 %= self.w
        y0 %= self.h
        x1 = (x0 + 1) % self.w
        y1 = (y0 + 1) % self.h
        a = self.a
        top = a[y0, x0] * (1 - fx) + a[y0, x1] * fx
        bot = a[y1, x0] * (1 - fx) + a[y1, x1] * fx
        return top * (1 - fy) + bot * fy


def image_pixels(img):
    w, h = img.size
    if w == 0 or h == 0:
        raise RuntimeError("Image '%s' has no pixel data (missing file?): %s" % (img.name, img.filepath))
    n = len(img.pixels)
    buf = np.empty(n, dtype=np.float32)
    img.pixels.foreach_get(buf)
    return buf.reshape(h, w, n // (w * h))


def downsample(a, max_size):
    h, w = a.shape[:2]
    f = max(1, int(math.ceil(max(h, w) / float(max_size))))
    if f == 1:
        return a
    h2, w2 = h // f, w // f
    a = a[:h2 * f, :w2 * f]
    return a.reshape(h2, f, w2, f, -1).mean(axis=(1, 3)) if a.ndim == 3 else a.reshape(h2, f, w2, f).mean(axis=(1, 3))


def resample_to(a, h, w):
    """Nearest resample of a texture-space array onto an h x w grid."""
    ys = ((np.arange(h) + 0.5) / h * a.shape[0]).astype(np.int32)
    xs = ((np.arange(w) + 0.5) / w * a.shape[1]).astype(np.int32)
    return a[ys][:, xs]


def box_sum(a, r):
    p = np.pad(a, r, mode="edge")
    c = np.pad(p.cumsum(0).cumsum(1), ((1, 0), (1, 0)))
    s = 2 * r + 1
    return c[s:, s:] - c[:-s, s:] - c[s:, :-s] + c[:-s, :-s]


def box_blur(a, r, passes=2):
    if r <= 0:
        return a
    s = float((2 * r + 1) ** 2)
    for _ in range(passes):
        a = box_sum(a, r) / s
    return a


def majority_filter(labels, k, r, iterations):
    if r <= 0:
        return labels
    for _ in range(iterations):
        counts = np.stack([box_sum((labels == j).astype(np.float32), r) for j in range(k)])
        labels = np.argmax(counts, axis=0).astype(labels.dtype)
    return labels


# ----------------------------------------------------------------------------------------------
# Material lookups

def upstream_image(socket, depth=0):
    if socket is None or not socket.is_linked or depth > 10:
        return None
    for link in socket.links:
        node = link.from_node
        if node.type == "TEX_IMAGE" and node.image:
            return node.image
        for i in node.inputs:
            found = upstream_image(i, depth + 1)
            if found:
                return found
    return None


def material_image_names(obj):
    """Names of every image texture in the object's materials."""
    names = []
    for slot in obj.material_slots:
        mat = slot.material
        if mat and mat.use_nodes:
            for n in mat.node_tree.nodes:
                if n.type == "TEX_IMAGE" and n.image and n.image.name not in names:
                    names.append(n.image.name)
    return names


def find_material_image(obj, kind):
    for slot in obj.material_slots:
        mat = slot.material
        if not mat or not mat.use_nodes:
            continue
        for n in mat.node_tree.nodes:
            if kind == "color" and n.type == "BSDF_PRINCIPLED":
                img = upstream_image(n.inputs.get("Base Color"))
                if img:
                    return img
            if kind == "normal" and n.type == "NORMAL_MAP":
                img = upstream_image(n.inputs.get("Color"))
                if img:
                    return img
    return None


# ----------------------------------------------------------------------------------------------
# Clustering

def assign(X, C, chunk=1 << 19):
    out = np.empty(len(X), dtype=np.int32)
    dist = np.empty(len(X), dtype=np.float32)
    for i in range(0, len(X), chunk):
        d = ((X[i:i + chunk, None, :] - C[None, :, :]) ** 2).sum(-1)
        out[i:i + chunk] = np.argmin(d, axis=1)
        dist[i:i + chunk] = np.sqrt(np.min(d, axis=1))
    return out, dist


def kmeans(X, k, iterations, rng, restarts=1):
    """k-means++ with restarts; keeps the solution with the lowest total squared distance."""
    best, best_cost = None, np.inf
    for _ in range(max(1, restarts)):
        C = _kmeans_once(X, k, iterations, rng)
        _, dist = assign(X, C)
        cost = float((dist.astype(np.float64) ** 2).sum())
        if cost < best_cost:
            best, best_cost = C, cost
    return best


def _kmeans_once(X, k, iterations, rng):
    first = X[rng.integers(len(X))]
    centers = [first]
    d2 = ((X - first) ** 2).sum(1)
    for _ in range(1, k):
        total = d2.sum()
        if total <= 0:
            break
        c = X[rng.choice(len(X), p=d2 / total)]
        centers.append(c)
        d2 = np.minimum(d2, ((X - c) ** 2).sum(1))
    C = np.array(centers)
    for _ in range(iterations):
        lab, _ = assign(X, C)
        newC = np.array([X[lab == j].mean(0) if np.any(lab == j) else C[j] for j in range(len(C))])
        if np.allclose(newC, C, atol=1e-5):
            break
        C = newC
    return C


def solid_map(feat_img, tolerance):
    """True where at least 3 of a texel's 8 neighbors share its color, so anti-aliased blend texels along an edge
    (usually 2 matching neighbors, along the edge) aren't taken as a third color, while bands 2 texels wide still count."""
    f = feat_img.astype(np.float32)
    close = np.zeros(f.shape[:2], dtype=np.int32)
    limit = (tolerance * 0.5) ** 2
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dx or dy:
                d = ((f - np.roll(np.roll(f, dy, axis=0), dx, axis=1)) ** 2).sum(-1)
                close += d < limit
    return close >= 3


def add_tolerance_regions(centers, feat, lum, solid, s, max_regions, rng):
    """Adds regions the area-driven clustering misses, one at a time (re-assigning after each):
    1. solid colors farther than color_tolerance from every region (small multi-color patches),
    2. tones of one color too far apart for Shade: darker than its light tone by darker_tolerance_stops (white vs black),
       or brighter by lighter_tolerance_stops (Shade can only darken)."""
    C = [np.asarray(c, dtype=np.float32) for c in centers]
    n_found = len(C)
    n_total = float(len(feat))
    min_count = max(20, int(s.min_color_share * n_total))
    log_y = np.log2(np.maximum(lum, 1e-5))
    while len(C) < max_regions:
        lab, dist = assign(feat, np.array(C, dtype=np.float32))
        new, why = None, ""
        if s.color_tolerance > 0:
            out = (dist > s.color_tolerance) & solid
            if out.sum() >= min_count:
                X = feat[out]
                sub = kmeans(X, min(3, len(X)), 15, rng)
                sl, _ = assign(X, sub)
                j = int(np.bincount(sl, minlength=len(sub)).argmax())
                new = X[sl == j].mean(0)
                why = "distinct color, %.2f%% of surface beyond color_tolerance" % (100 * out.mean())
        if new is None:
            best = None
            for c in range(len(C)):
                m = (lab == c) & solid
                if m.sum() < 2 * min_count:
                    continue
                ly = log_y[m]
                ref = np.percentile(ly, s.shade_reference_percentile)
                parts = []
                if s.darker_tolerance_stops > 0:
                    parts.append((ly < ref - s.darker_tolerance_stops, "darker"))
                if s.lighter_tolerance_stops > 0:
                    parts.append((ly > ref + s.lighter_tolerance_stops, "lighter"))
                for part, tag in parts:
                    if part.sum() >= min_count and (best is None or part.sum() > best[0]):
                        best = (int(part.sum()), feat[m][part].mean(0), tag)
            if best:
                new = best[1]
                why = "%s tone, %.2f%% of surface beyond the %s tolerance" % (best[2], 100 * best[0] / n_total, best[2])
        if new is None:
            break
        C.append(np.asarray(new, dtype=np.float32))
        log("tolerance: added a region (%s)" % why)
        # re-fit the region colors: a color that was absorbed by a neighbor tinted it (blue pulled black toward blue), and only
        # a clean neighbor lets the next distinct color stand out. Each region only averages texels within color_tolerance
        # of it, so a small region isn't dragged into a large one (a full k-means update would split the large one instead)
        Ca = np.array(C, dtype=np.float32)
        trim = s.color_tolerance if s.color_tolerance > 0 else np.inf
        for _ in range(10):
            lab, dist = assign(feat, Ca)
            near = dist < trim
            # only the regions found by clustering move; added ones stay at the color they were detected with
            # (otherwise dark shadows within tolerance of navy pull it to dark grey, and it takes over the shadows)
            Cn = np.array([feat[(lab == j) & near].mean(0) if j < n_found and np.any((lab == j) & near) else Ca[j]
                           for j in range(len(Ca))], dtype=np.float32)
            if np.allclose(Cn, Ca, atol=1e-5):
                break
            Ca = Cn
        C = list(Ca)
    else:
        lab, dist = assign(feat, np.array(C, dtype=np.float32))
        if s.color_tolerance > 0 and ((dist > s.color_tolerance) & solid).sum() >= min_count:
            log("WARNING: more distinct colors than the %d regions the convention allows" % max_regions)
    return np.array(C, dtype=np.float32)


def shade_groups(centers, feat, lin, s):
    """Groups region colors that are one color at different brightness: hue/saturation (a/L, b/L, lightness ignored)
    within shade_group_distance of a group member, and no more than darker_tolerance_stops below the group's lightest
    tone. Returns a group index per color, or None when off."""
    if s.shade_group_distance <= 0 or len(centers) < 2:
        return None
    C = np.asarray(centers, dtype=np.float32)
    lab, _ = assign(feat, C)
    lum = luminance(lin)
    ref = np.array([np.percentile(lum[lab == c], s.shade_reference_percentile) if np.any(lab == c) else 1e-6
                    for c in range(len(C))])
    group = -np.ones(len(C), dtype=np.int64)
    members = []                                         # per group: list of colors, the first is the lightest
    for c in np.argsort(-ref):
        for gi, mem in enumerate(members):
            close = min(np.linalg.norm(C[c, 1:] - C[m, 1:]) for m in mem) <= s.shade_group_distance
            gap = np.log2(max(ref[mem[0]], 1e-6) / max(ref[c], 1e-6))
            if close and (s.darker_tolerance_stops <= 0 or gap <= s.darker_tolerance_stops):
                group[c] = gi
                mem.append(c)
                break
        else:
            group[c] = len(members)
            members.append([c])
    return group


def merge_close_centers(X, C, distance):
    """Repeatedly joins the two closest region colors while they are within distance (sample-weighted mean)."""
    C = np.array(C, dtype=np.float32)
    merged = 0
    while distance > 0 and len(C) > 1:
        lab, _ = assign(X, C)
        n = np.bincount(lab, minlength=len(C)).astype(np.float64)
        D = np.sqrt(((C[:, None, :] - C[None, :, :]) ** 2).sum(-1))
        np.fill_diagonal(D, np.inf)
        i, j = np.unravel_index(np.argmin(D), D.shape)
        if D[i, j] >= distance:
            break
        C[i] = (C[i] * n[i] + C[j] * n[j]) / max(n[i] + n[j], 1.0)
        C = np.delete(C, j, axis=0)
        merged += 1
    return C, merged


# ----------------------------------------------------------------------------------------------
# Mesh helpers

_GRIDS = {}


def tri_grid(n):
    """Barycentric sample grid on a triangle with n steps per edge, its ring (boundary, in loop order) and corners."""
    if n in _GRIDS:
        return _GRIDS[n]
    pts, index = [], {}
    for a in range(n + 1):
        for b in range(n + 1 - a):
            c = n - a - b
            index[(a, b, c)] = len(pts)
            pts.append((a / n, b / n, c / n))
    ring = [index[(n - k, k, 0)] for k in range(n)]
    ring += [index[(0, n - k, k)] for k in range(n)]
    ring += [index[(k, 0, n - k)] for k in range(n)]
    corners = [index[(n, 0, 0)], index[(0, n, 0)], index[(0, 0, n)]]
    _GRIDS[n] = (np.array(pts), np.array(ring), corners)
    return _GRIDS[n]


def triangulate_non_tris(bm):
    faces = [f for f in bm.faces if len(f.verts) > 3]
    if faces:
        bmesh.ops.triangulate(bm, faces=faces, quad_method="BEAUTY", ngon_method="BEAUTY")


def tri_arrays(bm, uvl):
    bm.verts.index_update()
    bm.faces.index_update()
    faces = list(bm.faces)
    uv = np.empty((len(faces), 3, 2))
    vid = np.empty((len(faces), 3), dtype=np.int64)
    for i, f in enumerate(faces):
        ls = f.loops
        uv[i, 0] = ls[0][uvl].uv
        uv[i, 1] = ls[1][uvl].uv
        uv[i, 2] = ls[2][uvl].uv
        vid[i, 0] = ls[0].vert.index
        vid[i, 1] = ls[1].vert.index
        vid[i, 2] = ls[2].vert.index
    return faces, uv, vid


def world_coords(bm, m3):
    co = np.array([v.co[:] for v in bm.verts], dtype=np.float64)
    return co @ m3.T


def inset(pts, cen, amount):
    return pts + (cen - pts) * amount


# ----------------------------------------------------------------------------------------------
# Refinement: which faces need splitting

DETAIL_PRIORITY = 1e6                   # refinement priority of faces holding painted detail (letters, stripes)


def analyze_mixed_face(L, B2, ring, uv_tri, W, H, tol_px):
    """How much this face needs splitting to show its labels: 0 = one straight cut (or a 3-way junction) is enough,
    DETAIL_PRIORITY = it holds detail a cut can't show (a feature inside it, a stripe, a saddle), otherwise the cut's
    average deviation from the true boundary in multiples of tol_px (> 1)."""
    ring_labels = L[ring]
    if set(L.tolist()) - set(ring_labels.tolist()):
        return DETAIL_PRIORITY              # a feature lies inside the face without touching its edges
    R = len(ring)
    trans = [i for i in range(R) if ring_labels[i] != ring_labels[(i + 1) % R]]
    if not trans:
        return 0.0
    if len(trans) == 3:
        return 0.0                          # three-way junction: handled by a center vertex
    if len(trans) != 2:
        return DETAIL_PRIORITY              # 1 (inconsistent) or 4+ (stripe or saddle): split and look again
    t0, t1 = trans
    pa = (B2[ring[t0]] + B2[ring[(t0 + 1) % R]]) * 0.5
    pb = (B2[ring[t1]] + B2[ring[(t1 + 1) % R]]) * 0.5
    d = pb - pa
    side = d[0] * (B2[:, 1] - pa[1]) - d[1] * (B2[:, 0] - pa[0])
    ref_point = B2[ring[(t0 + 1) % R]]
    ref_side = d[0] * (ref_point[1] - pa[1]) - d[1] * (ref_point[0] - pa[0])
    if abs(ref_side) < 1e-9:
        return DETAIL_PRIORITY
    la = ring_labels[(t0 + 1) % R]
    lb = ring_labels[(t1 + 1) % R]
    pred = np.where(side * ref_side >= 0, la, lb)
    mismatch = np.count_nonzero(pred != L) / float(len(L))
    if mismatch == 0:
        return 0.0
    px = uv_tri * np.array([W, H])
    area_px = 0.5 * abs((px[1, 0] - px[0, 0]) * (px[2, 1] - px[0, 1]) - (px[2, 0] - px[0, 0]) * (px[1, 1] - px[0, 1]))
    bary3 = lambda p: np.array([1 - p[0] - p[1], p[0], p[1]])
    qa, qb = bary3(pa) @ px, bary3(pb) @ px
    length_px = max(np.linalg.norm(qb - qa), 1e-6)
    ratio = mismatch * area_px / length_px / max(tol_px, 1e-6)
    return ratio if ratio > 1.0 else 0.0


def faces_needing_refinement(uv, labelmap, dispmap, s, check_color=True, check_depth=True):
    """Returns (color, depth, mixed): refinement priorities per face (0 = fine as is), color from analyze_mixed_face and
    depth as how many times a flat face misses the fold by depth_subdiv_threshold, plus which faces a boundary crosses. Faces are sampled every sample_spacing_px texels (up to
    max_samples_per_edge per edge), so a painted stroke narrower than that spacing can go unseen on a large face."""
    W, H = labelmap.w, labelmap.h
    px = uv * np.array([W, H])
    elen = np.max(np.stack([np.linalg.norm(px[:, 1] - px[:, 0], axis=1),
                            np.linalg.norm(px[:, 2] - px[:, 1], axis=1),
                            np.linalg.norm(px[:, 0] - px[:, 2], axis=1)]), axis=0)
    n = np.clip(np.ceil(elen / s.sample_spacing_px), 2, s.max_samples_per_edge).astype(np.int32)
    color = np.zeros(len(uv))
    depth = np.zeros(len(uv))
    mixed = np.zeros(len(uv), dtype=bool)
    for nv in np.unique(n):
        group = np.nonzero(n == nv)[0]
        B, ring, corners = tri_grid(int(nv))
        B2 = B[:, 1:3]
        step = max(1, int(4_000_000 // max(len(B), 1)))      # bounded memory for densely sampled large faces
        for c0 in range(0, len(group), step):
            sel = group[c0:c0 + step]
            tris = uv[sel]
            pts = np.einsum("pk,fkd->fpd", B, tris)
            pts = inset(pts, tris.mean(1, keepdims=True), s.seam_inset)
            L = labelmap.nearest(pts)
            mixed_here = np.nonzero((L != L[:, :1]).any(1))[0]
            mixed[sel[mixed_here]] = True
            if check_color:
                for local in mixed_here:
                    color[sel[local]] = analyze_mixed_face(L[local], B2, ring, tris[local], W, H, s.boundary_tolerance_px)
            if check_depth and dispmap is not None:
                D = dispmap.bilinear(pts)
                lin = D[:, corners] @ B.T
                err = np.abs(D - lin).max(1) / max(s.depth_subdiv_threshold, 1e-9)
                depth[sel] = np.where(err > 1.0, err, 0.0)
    return color, depth, mixed


REFINE_SAFETY_TRIANGLES = 5_000_000      # generation stops refining here (memory safety); max_triangles is met by reduction
SPLIT_COST = 3.2                        # starting estimate: triangles added per face split (the face and its split neighbours)
CROSSED_GROWTH = 2.5                    # starting estimate: crossed faces a split crossed face leaves
CUT_COST = 2.0                          # triangles added per face a boundary is cut through


def refine(bm, uvl, m3, labelmap, dispmap, s, budget, cut_cost=None):
    """Color boundaries refine up to max_subdiv_levels, folds up to depth_subdiv_levels, so painted detail (letters)
    can get much finer faces than creases need. The projected size of the finished mesh (after the boundary cut) stays
    within budget; the split cost and how many crossed faces a split leaves are re-measured after every level."""
    depth_levels = s.depth_subdiv_levels if dispmap is not None else 0
    split_cost, crossed_growth = SPLIT_COST, CROSSED_GROWTH
    cut_cost = CUT_COST if cut_cost is None else cut_cost
    previous = None                     # (faces before, faces split, crossed before, crossed among the split)
    for level in range(max(s.max_subdiv_levels, depth_levels)):
        faces, uv, vid = tri_arrays(bm, uvl)
        cow = world_coords(bm, m3)
        elen = np.max(np.stack([np.linalg.norm(cow[vid[:, 1]] - cow[vid[:, 0]], axis=1),
                                np.linalg.norm(cow[vid[:, 2]] - cow[vid[:, 1]], axis=1),
                                np.linalg.norm(cow[vid[:, 0]] - cow[vid[:, 2]], axis=1)]), axis=0)
        color, depth, mixed = faces_needing_refinement(uv, labelmap, dispmap, s, check_color=level < s.max_subdiv_levels,
                                                       check_depth=level < depth_levels)
        big = elen > s.min_edge_length
        priority = np.maximum(color, depth) * big
        need = priority > 0
        count = int(need.sum())
        log("refine level %d: %d of %d faces need splitting (%d for color detail, %d for color edges, %d for depth)" % (
            level + 1, count, len(faces), int(((color >= DETAIL_PRIORITY) & big).sum()),
            int(((color > 0) & (color < DETAIL_PRIORITY) & big).sum()), int(((depth > 0) & (color == 0) & big).sum())))
        crossed = int(mixed.sum())
        if previous is not None:
            f0, n0, c0, cs0 = previous
            if n0 > 0:
                split_cost = max((len(faces) - f0) / float(n0), 1.0)
            if cs0 > 0:
                crossed_growth = max((crossed - (c0 - cs0)) / float(cs0), 1.0)
        if count == 0:
            break
        # the budget (safety limit) covers the finished refinement, so the projection includes the boundary cut that follows:
        # each split adds split_cost triangles, each crossed face gains CUT_COST when it is cut, and a split crossed face
        # turns into crossed_growth crossed faces
        crossed_share = float((mixed & need).sum()) / max(count, 1)
        per_split = split_cost + cut_cost * (crossed_growth - 1.0) * crossed_share
        room = int((budget - len(faces) - cut_cost * crossed) // max(per_split, 1e-6))
        if count > room:
            if room <= 0:
                log("WARNING: refinement safety limit reached, stopping refinement")
                break
            # spend what's left on the faces that need it most: painted detail first, then the worst-fitting cuts
            keep = np.argsort(-priority)[:room]
            need = np.zeros(len(faces), dtype=bool)
            need[keep] = True
            log("WARNING: refinement safety limit: splitting only the %d most needed of %d faces" % (room, count))
        previous = (len(faces), int(need.sum()), crossed, int((mixed & need).sum()))
        edges = set()
        for i in np.nonzero(need)[0]:
            edges.update(faces[i].edges)
        bmesh.ops.subdivide_edges(bm, edges=list(edges), cuts=1, use_grid_fill=True)
        triangulate_non_tris(bm)
    # crossed faces of the finished refinement, so the caller can measure what the cut really costs per crossed face
    _, uv, _ = tri_arrays(bm, uvl)
    return int(faces_needing_refinement(uv, labelmap, None, s, check_color=False, check_depth=False)[2].sum())


# ----------------------------------------------------------------------------------------------
# Cutting boundaries into edges

def cut_boundaries(bm, uvl, labelmap, s):
    W, H = labelmap.w, labelmap.h
    bm.edges.index_update()
    edges = list(bm.edges)
    eidx, flip, A, Bv, C = [], [], [], [], []
    for f in bm.faces:
        ls = f.loops
        cen = (ls[0][uvl].uv + ls[1][uvl].uv + ls[2][uvl].uv) / 3.0
        for l in ls:
            e = l.edge
            eidx.append(e.index)
            flip.append(l.vert != e.verts[0])
            A.append(l[uvl].uv[:])
            Bv.append(l.link_loop_next[uvl].uv[:])
            C.append(cen[:])
    eidx = np.array(eidx)
    flip = np.array(flip)
    A, Bv, C = np.array(A), np.array(Bv), np.array(C)
    length_px = np.linalg.norm((Bv - A) * np.array([W, H]), axis=1)
    m = np.clip(np.ceil(length_px / s.sample_spacing_px) + 1, 2, s.max_samples_per_edge + 1).astype(np.int32)

    # transitions per (edge, face side)
    side_count = np.zeros(len(eidx), dtype=np.int32)
    side_trans = [None] * len(eidx)
    for mv in np.unique(m):
        sel = np.nonzero(m == mv)[0]
        t = np.linspace(0.0, 1.0, int(mv))
        pts = A[sel, None, :] + (Bv[sel] - A[sel])[:, None, :] * t[None, :, None]
        pts = inset(pts, C[sel, None, :], s.seam_inset)
        L = labelmap.nearest(pts)
        change = L[:, 1:] != L[:, :-1]
        side_count[sel] = change.sum(1)
        for local in np.nonzero(change.any(1))[0]:
            i = sel[local]
            idx = np.nonzero(change[local])[0]
            side_trans[i] = [(t[j], t[j + 1], L[local, j]) for j in idx]

    # per edge, use the face side that sees the most crossings (they differ only across UV seams)
    best = {}
    for i in np.nonzero(side_count)[0]:
        e = eidx[i]
        if e not in best or side_count[i] > side_count[best[e]]:
            best[e] = i
    if not best:
        return

    rows, lo, hi, lab = [], [], [], []
    for e, i in best.items():
        for (a, b, l) in side_trans[i]:
            rows.append(i)
            lo.append(a)
            hi.append(b)
            lab.append(l)
    rows, lo, hi, lab = np.array(rows), np.array(lo), np.array(hi), np.array(lab)
    for _ in range(s.boundary_search_steps):
        mid = (lo + hi) * 0.5
        pts = A[rows] + (Bv[rows] - A[rows]) * mid[:, None]
        pts = inset(pts, C[rows], s.seam_inset)
        same = labelmap.nearest(pts) == lab
        lo = np.where(same, mid, lo)
        hi = np.where(same, hi, mid)
    tcut = (lo + hi) * 0.5
    tcut = np.where(flip[rows], 1.0 - tcut, tcut)

    plan = {}
    for r, t in zip(rows, tcut):
        plan.setdefault(int(eidx[r]), []).append(float(t))

    cut_verts = set()
    msf = s.min_split_fraction
    for ei, ts in plan.items():
        ts = sorted(t for t in ts if msf < t < 1.0 - msf)
        kept = []
        for t in ts:
            if not kept or t - kept[-1] > msf:
                kept.append(t)
        if not kept:
            continue
        e = edges[ei]
        v0, v1 = e.verts
        cur_e, cur_v, prev = e, v0, 0.0
        for t in kept:
            fac = (t - prev) / (1.0 - prev)
            _, nv = bmesh.utils.edge_split(cur_e, cur_v, fac)
            cut_verts.add(nv)
            cur_e = next(x for x in nv.link_edges if x.other_vert(nv) == v1)
            cur_v, prev = nv, t

    # connect the cut points across each face
    splits, pokes = [], []
    for f in bm.faces:
        vs = [l.vert for l in f.loops]
        idx = [i for i, v in enumerate(vs) if v in cut_verts]
        if len(idx) == 2:
            i, j = idx
            if (j - i) % len(vs) in (1, len(vs) - 1):
                pokes.append(f)
            else:
                splits.append((f, vs[i], vs[j]))
        elif len(idx) >= 3:
            pokes.append(f)
    for f, a, b in splits:
        bmesh.utils.face_split(f, a, b)
    if pokes:
        bmesh.ops.poke(bm, faces=pokes)
    triangulate_non_tris(bm)
    log("cut %d edges, %d faces split, %d junction faces" % (len(plan), len(splits), len(pokes)))


# ----------------------------------------------------------------------------------------------
# Face labels and cleanup

_LABEL_BARY = np.array([[1 / 3, 1 / 3, 1 / 3], [4 / 6, 1 / 6, 1 / 6], [1 / 6, 4 / 6, 1 / 6], [1 / 6, 1 / 6, 4 / 6]])


def label_faces(bm, uvl, labelmap, lab_layer):
    faces, uv, _ = tri_arrays(bm, uvl)
    pts = np.einsum("pk,fkd->fpd", _LABEL_BARY, uv)
    L = labelmap.nearest(pts)
    cnt = np.stack([(L == L[:, j:j + 1]).sum(1) for j in range(L.shape[1])], axis=1)
    best = L[np.arange(len(L)), np.argmax(cnt, axis=1)]
    for f, l in zip(faces, best):
        f[lab_layer] = int(l)


def merge_small_regions(bm, lab_layer, m3, min_frac, protected=(), protected_frac=0.0):
    """Islands smaller than min_frac of the surface join the neighbor they share the most edge with.
    Islands of a protected label (a color distinct from every other region) only merge below protected_frac."""
    if min_frac <= 0:
        return 0
    protected = set(protected)
    bm.faces.index_update()
    bm.verts.index_update()
    faces = list(bm.faces)
    cow = world_coords(bm, m3)
    area = np.empty(len(faces))
    for i, f in enumerate(faces):
        p = [cow[v.index] for v in f.verts]
        area[i] = 0.5 * np.linalg.norm(np.cross(p[1] - p[0], p[2] - p[0]))
    limit = area.sum() * min_frac
    limit_protected = area.sum() * min(protected_frac, min_frac)
    merged_total = 0
    for _ in range(8):
        comp = np.full(len(faces), -1, dtype=np.int64)
        comps = []
        for i in range(len(faces)):
            if comp[i] >= 0:
                continue
            label = faces[i][lab_layer]
            stack, members = [i], []
            comp[i] = len(comps)
            while stack:
                j = stack.pop()
                members.append(j)
                for e in faces[j].edges:
                    for g in e.link_faces:
                        if comp[g.index] < 0 and g[lab_layer] == label:
                            comp[g.index] = len(comps)
                            stack.append(g.index)
            comps.append(members)
        changed = 0
        for members in comps:
            label = faces[members[0]][lab_layer]
            if area[members].sum() >= (limit_protected if label in protected else limit):
                continue
            border = {}
            for j in members:
                for e in faces[j].edges:
                    for g in e.link_faces:
                        if g[lab_layer] != label:
                            border[g[lab_layer]] = border.get(g[lab_layer], 0.0) + e.calc_length()
            if not border:
                continue
            target = max(border, key=border.get)
            for j in members:
                faces[j][lab_layer] = target
            changed += 1
        merged_total += changed
        if changed == 0:
            break
    return merged_total


def vertex_displacement(bm, uvl, dispmap, s):
    """Per-vertex displacement (world units), averaged over the vertex's face corners, then smoothed."""
    bm.verts.index_update()
    uvs, cens, owners = [], [], []
    for f in bm.faces:
        ls = f.loops
        cen = sum((l[uvl].uv for l in ls), ls[0][uvl].uv * 0) / len(ls)
        for l in ls:
            uvs.append(l[uvl].uv[:])
            cens.append(cen[:])
            owners.append(l.vert.index)
    uvs, cens, owners = np.array(uvs), np.array(cens), np.array(owners)
    vals = dispmap.bilinear(inset(uvs, cens, s.seam_inset))
    nv = len(bm.verts)
    d = np.bincount(owners, weights=vals, minlength=nv) / np.maximum(np.bincount(owners, minlength=nv), 1)
    if s.depth_smooth_iterations > 0:
        ev = np.array([(e.verts[0].index, e.verts[1].index) for e in bm.edges])
        deg = np.bincount(ev.ravel(), minlength=nv)
        for _ in range(s.depth_smooth_iterations):
            acc = np.bincount(ev[:, 0], weights=d[ev[:, 1]], minlength=nv) + np.bincount(ev[:, 1], weights=d[ev[:, 0]], minlength=nv)
            d = 0.5 * d + 0.5 * np.where(deg > 0, acc / np.maximum(deg, 1), d)
    return np.clip(d, -s.depth_max, s.depth_max)


def safe_to_merge_around(v, uv_layers, eps=1e-5):
    """Dissolving v joins all its faces into one, and a face has one value per corner and one material.
    So every edge around v must be continuous in every UV map (no UV seam, which would put two islands in one face),
    the faces must share material and smoothing, and no edge may be marked as a seam or sharp."""
    faces = v.link_faces
    f0 = faces[0]
    for f in faces:
        if f.material_index != f0.material_index or f.smooth != f0.smooth:
            return False
    for e in v.link_edges:
        if e.seam or not e.smooth:
            return False
        ls = e.link_loops
        if len(ls) > 2:
            return False
        if len(ls) == 2:
            a0, a1, b0, b1 = ls[0], ls[0].link_loop_next, ls[1], ls[1].link_loop_next
            for layer in uv_layers:
                ua = {a0.vert: a0[layer].uv, a1.vert: a1[layer].uv}
                ub = {b0.vert: b0[layer].uv, b1.vert: b1[layer].uv}
                for vert, uv in ua.items():
                    if vert not in ub or (uv - ub[vert]).length > eps:
                        return False
    return True


def dissolve_unneeded(bm, uvl, lab_layer, is_original, dispmap, s, max_passes=8):
    """Removes refinement vertices that ended up inside one region.
    Each pass dissolves an independent set (no two chosen vertices share an edge), so every merge only rebuilds one
    vertex's ring of faces; large merged n-gons on curved cloth triangulate badly. Returns (dissolved, kept for seams)."""
    flag = bm.verts.layers.int.new("_rft_flag")          # 1 = original, 2 = needed for depth
    for v in bm.verts:
        v[flag] = 1 if is_original(v) else 0
    if dispmap is not None:
        d = vertex_displacement(bm, uvl, dispmap, s)
        for v in bm.verts:
            if v[flag] == 0:
                nb = [e.other_vert(v).index for e in v.link_edges]
                if nb and abs(d[v.index] - d[nb].mean()) > s.depth_subdiv_threshold:
                    v[flag] = 2
    uv_layers = list(bm.loops.layers.uv.values())
    total, kept_for_seams = 0, None
    for _ in range(max_passes):
        chosen, blocked, unsafe = [], set(), 0
        for v in bm.verts:
            if v[flag] or not v.link_faces or v in blocked:
                continue
            if len({f[lab_layer] for f in v.link_faces}) != 1:
                continue
            if not safe_to_merge_around(v, uv_layers):
                unsafe += 1
                continue
            chosen.append(v)
            blocked.update(e.other_vert(v) for e in v.link_edges)
        if kept_for_seams is None:
            kept_for_seams = unsafe
        if not chosen:
            break
        bmesh.ops.dissolve_verts(bm, verts=chosen, use_face_split=False, use_boundary_tear=False)
        triangulate_non_tris(bm)
        bm.verts.index_update()
        total += len(chosen)
    bm.verts.layers.int.remove(flag)
    return total, kept_for_seams or 0


def reduction_weights(bm, s):
    """Per-vertex freedom for the reduction (0 = keep, 1 = remove freely). 0 on region borders, open mesh borders,
    UV seams and marked seam/sharp edges, and (optionally) the source mesh's own vertices. Inside a region the weight is
    the region's area share relative to the largest region, to the power reduce_small_region_protection, lowered where
    Shade changes strongly between neighbors (reduce_shade_tolerance, in linear darkening)."""
    grp = bm.faces.layers.int["RegionGroup"]
    sh = bm.loops.layers.float_color.get("Shade")
    orig = bm.verts.layers.int.get("_rft_orig")
    uv_layers = list(bm.loops.layers.uv.values())
    bm.verts.index_update()
    nv = len(bm.verts)
    vshade = np.zeros(nv)
    if sh is not None:
        for v in bm.verts:
            ls = v.link_loops
            if ls:
                vshade[v.index] = sum(l[sh][0] for l in ls) / len(ls)
        if s.precompensate:
            vshade = srgb_to_linear(vshade).astype(np.float64)
    area = {}
    for f in bm.faces:
        area[f[grp]] = area.get(f[grp], 0.0) + f.calc_area()
    biggest = max(area.values()) if area else 1.0
    region_scale = {g: max((a / biggest) ** s.reduce_small_region_protection, 0.02) for g, a in area.items()}
    w = np.ones(nv)
    for e in bm.edges:
        fs = e.link_faces
        locked = len(fs) != 2 or fs[0][grp] != fs[1][grp] or e.seam or not e.smooth             or fs[0].material_index != fs[1].material_index
        if not locked:
            la, lb = e.link_loops
            for layer in uv_layers:          # UV seam: the two faces disagree on a corner's UV
                ua = {la.vert: la[layer].uv, la.link_loop_next.vert: la.link_loop_next[layer].uv}
                ub = {lb.vert: lb[layer].uv, lb.link_loop_next.vert: lb.link_loop_next[layer].uv}
                if any(vert not in ub or (uv - ub[vert]).length > 1e-5 for vert, uv in ua.items()):
                    locked = True
                    break
        a, b = e.verts
        if locked:
            w[a.index] = 0.0
            w[b.index] = 0.0
        elif sh is not None and s.reduce_shade_tolerance > 0:
            f = 1.0 / (1.0 + abs(vshade[a.index] - vshade[b.index]) / s.reduce_shade_tolerance)
            w[a.index] = min(w[a.index], f)
            w[b.index] = min(w[b.index], f)
    for v in bm.verts:
        if w[v.index] == 0.0:
            continue
        if s.reduce_keep_original_vertices and orig is not None and v[orig]:
            w[v.index] = 0.0
            continue
        fs = v.link_faces
        w[v.index] *= region_scale.get(fs[0][grp], 1.0) if fs else 0.0
    return w


def reduce_to_budget(bm, s, vertex_group_names):
    """Reduces the finished mesh to max_triangles with Blender's collapse decimation (quadric error: flat, uniform areas
    go first, creases last), steered by reduction_weights: edges between two weight-0 vertices (region borders, seams)
    are never collapsed, and lower weights make collapsing more expensive (reduce_protection_strength).
    Returns a new bmesh (the input is freed)."""
    if s.max_triangles <= 0 or len(bm.faces) <= s.max_triangles:
        return bm
    start = len(bm.faces)
    weights = reduction_weights(bm, s)
    tmp_me = bpy.data.meshes.new("_rft_reduce")
    bm.to_mesh(tmp_me)
    tmp = bpy.data.objects.new("_rft_reduce", tmp_me)
    bpy.context.scene.collection.objects.link(tmp)
    try:
        for name in vertex_group_names:                  # keep bone-weight indices lined up
            tmp.vertex_groups.new(name=name)
        vg = tmp.vertex_groups.new(name="_rft_reduce_weight")
        levels = np.round(weights * 50) / 50.0
        for value in np.unique(levels):
            idx = np.nonzero(levels == value)[0].tolist()
            if value > 0 and idx:
                vg.add(idx, float(value), "REPLACE")
        mod = tmp.modifiers.new("reduce", "DECIMATE")
        mod.decimate_type = "COLLAPSE"
        mod.ratio = min(1.0, s.max_triangles / float(start) * 0.995)
        mod.vertex_group = vg.name
        mod.vertex_group_factor = s.reduce_protection_strength
        mod.use_collapse_triangulate = True
        depsgraph = bpy.context.evaluated_depsgraph_get()
        out_me = bpy.data.meshes.new_from_object(tmp.evaluated_get(depsgraph))
        nb = bmesh.new()
        nb.from_mesh(out_me)
        bpy.data.meshes.remove(out_me)
        dl = nb.verts.layers.deform.active
        if dl is not None:                               # drop the helper group's weights
            gi = vg.index
            for v in nb.verts:
                if gi in v[dl]:
                    del v[dl][gi]
    finally:
        bpy.data.objects.remove(tmp)
        bpy.data.meshes.remove(tmp_me)
    bm.free()
    log("reduced %d -> %d triangles (max_triangles %d); %.0f%% of vertices locked (borders, seams, originals)" % (
        start, len(nb.faces), s.max_triangles, 100.0 * float((weights == 0).mean())))
    return nb


def fill_missing_weights(bm, new_verts):
    """Safety net: a new vertex with no bone weights takes the average of its neighbors."""
    dl = bm.verts.layers.deform.active
    if dl is None:
        return 0
    fixed = 0
    for v in new_verts:
        if not v.is_valid or len(v[dl]) > 0:
            continue
        acc, n = {}, 0
        for e in v.link_edges:
            o = e.other_vert(v)
            if len(o[dl]) == 0:
                continue
            n += 1
            for g, w in o[dl].items():
                acc[g] = acc.get(g, 0.0) + w
        for g, w in acc.items():
            v[dl][g] = w / n
        fixed += 1 if n else 0
    return fixed


# ----------------------------------------------------------------------------------------------
# Main

def parse_index_list(text):
    return [int(x) for x in text.replace(" ", "").split(",") if x != ""]


def depth_clusters(text, palette_lin, index_of):
    """depth_regions entries: '#rrggbb' picks the region nearest that color, an integer picks that region index."""
    out = []
    for item in text.replace(" ", "").split(","):
        if not item:
            continue
        if item.startswith("#"):
            target = linear_to_oklab(hex_to_linear(item))
            out.append(int(np.argmin([np.linalg.norm(linear_to_oklab(np.asarray(p)) - target) for p in palette_lin])))
        else:
            out += [c for c, i in index_of.items() if i == int(item)]
    return out


def set_color_attributes(me, active_name, render_name):
    try:
        if active_name and active_name in me.color_attributes:
            me.color_attributes.active_color = me.color_attributes[active_name]
        if render_name and render_name in me.color_attributes:
            me.color_attributes.render_color_index = me.color_attributes.find(render_name)
    except Exception:
        pass


def is_region_group(name, prefix):
    return name.startswith(prefix) and name[len(prefix):].isdigit()


def face_loop_arrays(me):
    starts = np.empty(len(me.polygons), dtype=np.int64)
    totals = np.empty(len(me.polygons), dtype=np.int64)
    me.polygons.foreach_get("loop_start", starts)
    me.polygons.foreach_get("loop_total", totals)
    order = np.argsort(starts, kind="stable")          # a face's corners are contiguous; faces sorted by first corner
    return np.repeat(order, totals[order])


def write_region_groups(obj, prefix):
    """One vertex group per region from the RegionGroup face attribute. Boundary vertices belong to both sides."""
    me = obj.data
    for vg in list(obj.vertex_groups):
        if is_region_group(vg.name, prefix):
            obj.vertex_groups.remove(vg)
    grp = np.empty(len(me.polygons), dtype=np.int32)
    me.attributes["RegionGroup"].data.foreach_get("value", grp)
    lv = np.empty(len(me.loops), dtype=np.int64)
    me.loops.foreach_get("vertex_index", lv)
    loop_group = grp[face_loop_arrays(me)]
    present = sorted(set(grp.tolist()))
    mask = np.zeros(len(me.vertices), dtype=np.int32)
    for g in present:
        verts = np.unique(lv[loop_group == g])
        mask[verts] |= 1 << g
        obj.vertex_groups.new(name="%s%d" % (prefix, g)).add(verts.tolist(), 1.0, "REPLACE")
    # membership as generated, so apply_ids can tell hand edits (faces assigned to / removed from groups) from boundaries
    if "RegionGroupMask" in me.attributes:
        me.attributes.remove(me.attributes["RegionGroupMask"])
    me.attributes.new("RegionGroupMask", "INT", "POINT").data.foreach_set("value", mask)
    log("wrote %d vertex groups (%s1 = most faces)" % (len(present), prefix))


def apply_region_ids(obj, s):
    """Writes RegionID/RegionIndex from a group -> region ID mapping.
    Hand edits to the vertex groups are respected: compared with the membership recorded when the groups were written
    (RegionGroupMask), a face whose vertices were all assigned to a group goes to that group, and a face removed from its
    group goes to another group that still holds all of its vertices. Faces that merely touch boundaries on every side
    (their vertices sit in several groups from the start) keep their group."""
    me = obj.data
    if "RegionGroup" not in me.attributes:
        raise RuntimeError("%s has no RegionGroup attribute; run the region pass first" % obj.name)
    mapping = {}
    for item in s.apply_ids.replace(" ", "").split(","):
        if not item:
            continue
        g, i = item.split(":")
        mapping[int(g[len(s.group_prefix):] if g.startswith(s.group_prefix) else g)] = int(i)
    table = region_value_table(s.convention)
    if mapping and max(mapping.values()) >= len(table):
        raise RuntimeError("region ID exceeds the %s convention's %d regions" % (s.convention, len(table)))

    grp = np.empty(len(me.polygons), dtype=np.int32)
    me.attributes["RegionGroup"].data.foreach_get("value", grp)
    groups = {vg.index: int(vg.name[len(s.group_prefix):]) for vg in obj.vertex_groups if is_region_group(vg.name, s.group_prefix)}
    now = np.zeros(len(me.vertices), dtype=np.int64)
    for v in me.vertices:
        for g in v.groups:
            if g.group in groups and g.weight > 0:
                now[v.index] |= 1 << groups[g.group]
    if "RegionGroupMask" in me.attributes:
        orig = np.empty(len(me.vertices), dtype=np.int32)
        me.attributes["RegionGroupMask"].data.foreach_get("value", orig)
        orig = orig.astype(np.int64)
    else:
        orig = now.copy()
    bits = lambda m: {b for b in range(1, 63) if m >> b & 1}
    ambiguous = moved = 0
    for p in me.polygons:
        vs = list(p.vertices)
        common_now = int(np.bitwise_and.reduce(now[vs]))
        common_orig = int(np.bitwise_and.reduce(orig[vs]))
        g0 = int(grp[p.index])
        added = bits(common_now & ~common_orig)
        if added:
            g = max(added)                  # faces assigned to a group by hand go there
        elif common_now >> g0 & 1:
            g = g0                          # still in its own group (boundary-only faces sit in several and stay put)
            ambiguous += len(bits(common_now)) > 1
        elif common_now:
            g = max(bits(common_now))       # removed from its group by hand: the remaining group that holds it
        else:
            g = g0                          # no group holds the whole face: keep the attribute
        if g != grp[p.index]:
            moved += 1
            grp[p.index] = g
    me.attributes["RegionGroup"].data.foreach_set("value", grp)

    missing = sorted(set(grp.tolist()) - set(mapping))
    if missing:
        log("WARNING: groups %s have no ID in apply_ids and get region 0" % missing)
    ids = np.array([mapping.get(int(g), 0) for g in grp], dtype=np.int32)
    for name in ("RegionID", "RegionIndex"):
        if name in me.attributes:
            me.attributes.remove(me.attributes[name])
    me.attributes.new("RegionIndex", "INT", "FACE").data.foreach_set("value", ids)
    values = np.array(table, dtype=np.float32)[ids[face_loop_arrays(me)]]
    cols = np.zeros((len(me.loops), 4), dtype=np.float32)
    cols[:, 0] = values
    cols[:, 3] = 1.0
    me.color_attributes.new("RegionID", "FLOAT_COLOR", "CORNER").data.foreach_set("color", cols.ravel())
    set_color_attributes(me, None, "RegionID")
    me.update()
    log("RegionID written on %s: %d faces, %d moved to match edited groups, %d boundary-only faces resolved"
        % (obj.name, len(me.polygons), moved, ambiguous))
    return {"faces": len(me.polygons), "moved": moved, "ambiguous": ambiguous,
            "ids": {"%s%d" % (s.group_prefix, g): i for g, i in sorted(mapping.items())}}


def run(s, save_path="", report_path=""):
    global _T0
    _T0 = time.time()
    if bpy.context.object and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    src = bpy.data.objects.get(s.object_name) if s.object_name else bpy.context.active_object
    if src is None or src.type != "MESH":
        raise RuntimeError("No mesh object (set object_name or select one)")
    if s.apply_ids:
        result = apply_region_ids(src, s)
        if save_path:
            bpy.ops.wm.save_as_mainfile(filepath=save_path)
        return src, result
    img = bpy.data.images.get(s.image_name) if s.image_name else find_material_image(src, "color")
    if img is None:
        raise RuntimeError("No color texture found (set image_name)")
    uv_name = s.uv_layer or src.data.uv_layers.active.name
    log("object %s, image %s %s, uv %s" % (src.name, img.name, tuple(img.size), uv_name))
    used = material_image_names(src)
    if s.image_name and used and img.name not in used:
        log("WARNING: texture %s isn't used by %s's material (it uses %s); the regions will follow %s through this mesh's UVs"
            % (img.name, src.name, ", ".join(used[:3]), img.name))

    # --- texture analysis -----------------------------------------------------------------------
    px = image_pixels(img)[..., :3]
    lin = px if img.is_float else srgb_to_linear(px)
    lin = downsample(lin, s.analysis_size).astype(np.float32)
    H, W = lin.shape[:2]
    feat_full = color_features(lin.reshape(-1, 3), s.lightness_weight)
    color_map = Map(lin)

    # area-weighted surface samples
    me = src.data
    me.calc_loop_triangles()
    nt = len(me.loop_triangles)
    tl = np.empty(nt * 3, dtype=np.int32)
    me.loop_triangles.foreach_get("loops", tl)
    tv = np.empty(nt * 3, dtype=np.int32)
    me.loop_triangles.foreach_get("vertices", tv)
    uvd = np.empty(len(me.loops) * 2, dtype=np.float32)
    me.uv_layers[uv_name].data.foreach_get("uv", uvd)
    co = np.empty(len(me.vertices) * 3, dtype=np.float32)
    me.vertices.foreach_get("co", co)
    m3 = np.array(src.matrix_world.to_3x3(), dtype=np.float64)
    cow = co.reshape(-1, 3) @ m3.T
    tri_uv = uvd.reshape(-1, 2)[tl].reshape(nt, 3, 2)
    tri_co = cow[tv].reshape(nt, 3, 3)
    tri_area = 0.5 * np.linalg.norm(np.cross(tri_co[:, 1] - tri_co[:, 0], tri_co[:, 2] - tri_co[:, 0]), axis=1)
    rng = np.random.default_rng(s.seed)
    pick = rng.choice(nt, size=s.cluster_samples, p=tri_area / tri_area.sum())
    r1, r2 = rng.random(s.cluster_samples), rng.random(s.cluster_samples)
    sq = np.sqrt(r1)
    bary = np.stack([1 - sq, sq * (1 - r2), sq * r2], axis=1)
    sample_uv = np.einsum("nk,nkd->nd", bary, tri_uv[pick])
    sample_lin = color_map.nearest(sample_uv)
    sample_feat = color_features(sample_lin, s.lightness_weight)

    # cluster
    max_regions = len(region_value_table(s.convention))
    if s.palette:
        centers = np.array([color_features(hex_to_linear(h), s.lightness_weight) for h in s.palette.split(",")])
    elif s.region_count > 0:
        centers = kmeans(sample_feat, min(s.region_count, max_regions), s.kmeans_iterations, rng, s.kmeans_restarts)
    else:
        trials = []
        for k in range(1, max_regions + 1):
            trial = kmeans(sample_feat, k, s.kmeans_iterations, rng, s.kmeans_restarts)
            _, dist = assign(sample_feat, trial)
            spread = float(np.percentile(dist, s.auto_percentile))
            log("auto: %d regions -> %.1f%% of surface within %.3f" % (k, s.auto_percentile, spread))
            trials.append((trial, spread))
            if spread <= s.merge_distance:
                break
        centers = trials[-1][0]
        if trials[-1][1] > s.merge_distance:
            # never met: anti-aliased edge texels set a floor; take the fewest regions that come close to the best spread
            floor = min(t[1] for t in trials)
            k_pick = next(i for i, t in enumerate(trials) if t[1] <= floor * (1.0 + s.auto_min_gain))
            centers = trials[k_pick][0]
            log("auto: merge_distance %.3f is below the floor %.3f; using %d regions" % (s.merge_distance, floor, k_pick + 1))
    if not s.palette and (s.color_tolerance > 0 or s.darker_tolerance_stops > 0 or s.lighter_tolerance_stops > 0):
        sample_solid = Map(solid_map(feat_full.reshape(H, W, 3), max(s.color_tolerance, 0.05))).nearest(sample_uv)
        centers = add_tolerance_regions(centers, sample_feat, luminance(sample_lin), sample_solid, s, max_regions, rng)
    centers, joined = merge_close_centers(sample_feat, centers, s.region_merge_distance)
    if joined:
        log("region_merge_distance joined %d region colors" % joined)
    k = len(centers)
    if k > max_regions:
        raise RuntimeError("%d regions exceed the %s convention's %d" % (k, s.convention, max_regions))

    labels, _ = assign(feat_full, centers.astype(np.float32))
    group_of = shade_groups(centers, sample_feat, sample_lin, s)
    if group_of is not None and group_of.max() + 1 < k:
        # every texel keeps its nearest color's boundaries, then takes that color's group: tones of one color become Shade
        n_groups = int(group_of.max()) + 1
        labels = group_of[labels]
        sl, _ = assign(sample_feat, centers.astype(np.float32))
        sg = group_of[sl]
        centers = np.array([sample_feat[sg == j].mean(0) for j in range(n_groups)], dtype=np.float32)
        log("shade_group_distance grouped %d region colors into %d regions" % (k, n_groups))
        k = n_groups
    labels = labels.reshape(H, W).astype(np.int8)
    labels = majority_filter(labels, k, s.majority_radius, s.majority_iterations)
    labelmap = Map(labels)

    # region order: largest surface area first, then the optional remap onto the kit plan
    sample_lab = labelmap.nearest(sample_uv)
    area_share = np.bincount(sample_lab, minlength=k) / float(len(sample_lab))
    order = list(np.argsort(-area_share))
    index_of = {int(c): i for i, c in enumerate(order)}
    if s.index_remap:
        remap = dict(tuple(int(x) for x in p.split(":")) for p in s.index_remap.split(","))
        index_of = {c: remap.get(i, i) for c, i in index_of.items()}
        if len(set(index_of.values())) != len(index_of):
            raise RuntimeError("index_remap maps two regions onto one index")
    if max(index_of.values()) >= max_regions:
        raise RuntimeError("region index exceeds the %s convention" % s.convention)

    # shade: luminance below the region's unshaded tone
    Y = luminance(lin)
    sample_Y = luminance(sample_lin)
    y_ref = np.array([np.percentile(sample_Y[sample_lab == c], s.shade_reference_percentile) if np.any(sample_lab == c) else 1.0
                      for c in range(k)])
    shade = np.clip((y_ref[labels] - Y) / np.maximum(y_ref[labels] * s.shade_range, 1e-6), 0.0, 1.0).astype(np.float32)
    shade_map = Map(shade)
    sample_shade = shade_map.nearest(sample_uv)
    # how much shade_range each region needs so its darkest 5% of tones aren't clipped at Shade 1
    shade_needed = [float(np.clip(1 - np.percentile(sample_Y[sample_lab == c], 5) / max(y_ref[c], 1e-6), 0, 1))
                    if np.any(sample_lab == c) else 0.0 for c in range(k)]
    log("shade_range needed per region (darkest 5%% of tones): %s; current %.2f" % (
        ", ".join("%.2f" % v for v in shade_needed), s.shade_range))
    palette_lin = []
    for c in range(k):
        mask = (sample_lab == c) & (sample_shade <= 0.1)
        if not np.any(mask):
            mask = sample_lab == c
        palette_lin.append(sample_lin[mask].mean(0) if np.any(mask) else np.zeros(3))

    # depth map (world units, masked to the depth regions)
    dispmap = None
    if s.depth_source != "none":
        if s.depth_source == "luminance":
            fold = shade
        else:
            dimg = bpy.data.images.get(s.depth_image) if s.depth_image else find_material_image(src, "normal")
            if dimg is None:
                raise RuntimeError("depth_source '%s' needs depth_image" % s.depth_source)
            raw = image_pixels(dimg)[..., :3]          # raw values: height/normal maps are non-color data
            raw = downsample(raw, max(s.analysis_size, 1))
            if s.depth_source == "height":
                h = luminance(raw) - s.depth_midlevel
                fold = -np.clip(h / max(s.depth_midlevel, 1e-6), -1, 1)
            elif s.depth_source == "normal":
                n = raw * 2.0 - 1.0
                nz = np.clip(n[..., 2], 0.1, None)
                ny = -n[..., 1] if s.normal_green == "directx" else n[..., 1]
                p_, q_ = -n[..., 0] / nz, -ny / nz
                hh, ww = p_.shape
                WX, WY = np.meshgrid(np.fft.fftfreq(ww) * 2 * np.pi, np.fft.fftfreq(hh) * 2 * np.pi)
                den = WX ** 2 + WY ** 2
                den[0, 0] = 1.0
                Z = (-1j * WX * np.fft.fft2(p_) - 1j * WY * np.fft.fft2(q_)) / den
                Z[0, 0] = 0
                z = np.real(np.fft.ifft2(Z))
                z = z - box_blur(z, s.depth_highpass_px)
                fold = -z / max(np.percentile(np.abs(z), 99), 1e-6)
            else:
                raise RuntimeError("unknown depth_source " + s.depth_source)
            fold = resample_to(np.clip(fold, -1, 1), H, W)
        if s.depth_threshold > 0:
            t = min(s.depth_threshold, 0.99)
            fold = np.sign(fold) * np.clip((np.abs(fold) - t) / (1.0 - t), 0.0, 1.0)
        fold = np.sign(fold) * np.abs(fold) ** s.depth_curve
        fold = box_blur(fold.astype(np.float64), s.depth_blur_px)
        hard = np.ones(fold.shape, dtype=np.float64)
        if s.depth_regions:
            region_mask = np.isin(labels, depth_clusters(s.depth_regions, palette_lin, index_of))
            # ramps to zero at the region's edge from the inside, so neighbors never move or get refined
            hard = region_mask.astype(np.float64)
            fold = fold * box_blur(hard, max(s.depth_edge_falloff_px, 1)) * hard
        if s.depth_preserve_volume > 0:
            # luminance folds only ever point inward, so on their own they shrink the whole surface (most texels are a little
            # darker than the region's light tone). Removing the local average makes creases go in and the cloth between them
            # come out by the same amount, so the surface keeps its volume. The average only counts texels of displaced regions.
            r = max(s.depth_volume_radius_px, 1)
            local = box_blur(fold, r, passes=1) / np.maximum(box_blur(hard, r, passes=1), 1e-6)
            fold = fold - s.depth_preserve_volume * local * hard
        dispmap = Map(np.clip(-fold * s.depth_strength, -s.depth_max, s.depth_max))

    # --- mesh ---------------------------------------------------------------------------------
    if s.in_place:
        obj = src
    else:
        obj = src.copy()
        obj.data = src.data.copy()
        obj.name = src.name + s.output_suffix
        obj.data.name = src.data.name + s.output_suffix
        for col in src.users_collection:
            col.objects.link(obj)
        if s.hide_source:
            src.hide_set(True)
    me = obj.data
    for name in ("RegionID", "Shade", "RegionPreview", "RegionIndex", "RegionGroup", "RegionGroupMask"):
        a = me.attributes.get(name)
        if a:
            me.attributes.remove(a)

    bm = bmesh.new()
    bm.from_mesh(me)
    uvl = bm.loops.layers.uv[uv_name]
    tris_before = sum(len(f.verts) - 2 for f in bm.faces)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3], quad_method="BEAUTY", ngon_method="BEAUTY")
    # BMVert wrappers don't keep their identity across bmesh.ops calls, so original vertices are recognised by position
    bm.verts.index_update()
    kd = mathutils.kdtree.KDTree(len(bm.verts))
    for v in bm.verts:
        kd.insert(v.co, v.index)
    kd.balance()
    dims = [max(v.co[i] for v in bm.verts) - min(v.co[i] for v in bm.verts) for i in range(3)]
    same_eps = max(max(dims), 1e-6) * 1e-7
    is_original = lambda v: kd.find(v.co)[2] <= same_eps

    # generation isn't capped by max_triangles (the reduction pass at the end is); a high safety limit only stops runaway settings
    island_layer = None
    if s.method == "targeted":
        island_layer = _targeted.targeted_inject(bm, uvl, labelmap, dispmap, s, faces_needing_refinement, tri_arrays, log)
        triangulate_non_tris(bm)
        log("%d triangles after injecting detail islands" % len(bm.faces))
    else:
        refine(bm, uvl, m3, labelmap, dispmap, s, REFINE_SAFETY_TRIANGLES)
        cut_boundaries(bm, uvl, labelmap, s)
        log("%d triangles after refinement and the boundary cut" % len(bm.faces))
    lab_layer = bm.faces.layers.int.new("_rft_label")
    label_faces(bm, uvl, labelmap, lab_layer)
    distinct = []
    if s.color_tolerance > 0 and k > 1:
        cd = np.sqrt(((centers[:, None, :] - centers[None, :, :]) ** 2).sum(-1))
        np.fill_diagonal(cd, np.inf)
        distinct = [c for c in range(k) if cd[c].min() > s.color_tolerance]
    merged = merge_small_regions(bm, lab_layer, m3, s.min_region_area, distinct, s.min_color_share)
    log("merged %d small islands" % merged)

    dissolved = 0
    if s.dissolve_unneeded and s.method == "targeted":
        log("dissolve_unneeded skipped: targeted islands only hold vertices they need")
    elif s.dissolve_unneeded:
        dissolved, kept_for_seams = dissolve_unneeded(bm, uvl, lab_layer, is_original, dispmap, s)
        log("dissolved %d unneeded vertices (%d kept on UV seams / material / sharp edges)" % (dissolved, kept_for_seams))
    else:
        log("dissolve_unneeded off: all refinement vertices kept")

    new_verts = [v for v in bm.verts if not is_original(v)]
    filled = fill_missing_weights(bm, new_verts)

    # --- groups: numbered by final face count, most faces first -------------------------------
    face_count = {}
    for f in bm.faces:
        face_count[f[lab_layer]] = face_count.get(f[lab_layer], 0) + 1
    group_order = sorted(face_count, key=lambda c: -face_count[c])
    group_of = {int(c): g + 1 for g, c in enumerate(group_order)}
    for c in group_order:
        log("group %s%d: %s  %d faces  %.1f%% of surface" % (s.group_prefix, group_of[c], linear_to_hex(palette_lin[c]),
                                                         face_count[c], 100 * area_share[c]))

    # --- attributes ---------------------------------------------------------------------------
    table = region_value_table(s.convention)
    grp_layer = bm.faces.layers.int.new("RegionGroup")
    idx_layer = bm.faces.layers.int.new("RegionIndex") if s.write_region_ids else None
    reg_layer = bm.loops.layers.float_color.new("RegionID") if s.write_region_ids else None
    shade_layer = bm.loops.layers.float_color.new("Shade") if s.write_shade else None
    prev_layer = bm.loops.layers.float_color.new("RegionPreview") if s.write_preview else None
    loops, uvs, cens = [], [], []
    for f in bm.faces:
        c = f[lab_layer]
        f[grp_layer] = group_of[c]
        if reg_layer:
            f[idx_layer] = index_of[c]
            r = table[index_of[c]]
        col = palette_lin[c]
        ls = f.loops
        cen = (ls[0][uvl].uv + ls[1][uvl].uv + ls[2][uvl].uv) / 3.0
        for l in ls:
            if reg_layer:
                l[reg_layer] = (r, 0.0, 0.0, 1.0)
            if prev_layer:
                l[prev_layer] = (float(col[0]), float(col[1]), float(col[2]), 1.0)
            loops.append(l)
            uvs.append(l[uvl].uv[:])
            cens.append(cen[:])
    if shade_layer:
        sv = shade_map.nearest(inset(np.array(uvs), np.array(cens), s.seam_inset))
        if s.precompensate:
            sv = linear_to_srgb(sv)
        for l, v in zip(loops, sv):
            l[shade_layer] = (float(v), float(v), float(v), 1.0)
    bm.faces.layers.int.remove(lab_layer)

    # original vertices, flagged before depth moves them (the position lookup can't find them afterwards)
    orig_layer = bm.verts.layers.int.new("_rft_orig")
    for v in bm.verts:
        v[orig_layer] = 1 if is_original(v) else 0

    # --- depth ----------------------------------------------------------------------------------
    if dispmap is not None:
        d = vertex_displacement(bm, uvl, dispmap, s)
        bm.normal_update()
        # thickness limit: measured on the undisplaced surface, straight through the part along the inward normal
        bvh = mathutils.bvhtree.BVHTree.FromBMesh(bm) if s.depth_max_thickness < 1.0 else None
        eps = same_eps * 1e3
        moves, limited = [], 0
        isl = bm.verts.layers.int.get(island_layer) if island_layer else None
        for v in bm.verts:
            dv = d[v.index]
            if isl is not None and v[isl] != 1:
                continue                                    # targeted: only island interiors move
            if abs(dv) < 1e-7:
                continue
            n = v.normal.copy()
            scale = np.linalg.norm(m3 @ np.array(n[:]))
            amount = dv / max(scale, 1e-9)                  # object units
            if bvh is not None and amount < 0:
                hit = bvh.ray_cast(v.co - n * eps, -n)
                if hit[0] is not None:
                    limit = s.depth_max_thickness * (hit[3] + eps)
                    if -amount > limit:
                        amount = -limit
                        limited += 1
            moves.append((v, n * amount))
        for v, offset in moves:
            v.co += offset
        log("displaced %d vertices (max %.4f), %d limited by thickness" % (len(moves), float(np.abs(d).max()), limited))

    # --- reduction to max_triangles -------------------------------------------------------------
    bm = reduce_to_budget(bm, s, [vg.name for vg in obj.vertex_groups])
    bm.verts.layers.int.remove(bm.verts.layers.int["_rft_orig"])
    if island_layer and bm.verts.layers.int.get(island_layer) is not None and not DEBUG_KEEP_LAYERS:
        bm.verts.layers.int.remove(bm.verts.layers.int[island_layer])

    tris_after = len(bm.faces)
    bm.to_mesh(me)
    bm.free()
    me.update()
    set_color_attributes(me, "RegionPreview" if s.write_preview else None, "RegionID" if s.write_region_ids else None)
    if s.write_vertex_groups:
        write_region_groups(obj, s.group_prefix)

    if s.decimate_ratio < 1.0:
        vg =obj.vertex_groups.get("_RegionInterior") or obj.vertex_groups.new(name="_RegionInterior")
        face_idx = np.empty(len(me.polygons), dtype=np.int32)
        me.attributes["RegionGroup"].data.foreach_get("value", face_idx)
        vert_regions = {}
        for p, ri in zip(me.polygons, face_idx):
            for vi in p.vertices:
                vert_regions.setdefault(vi, set()).add(int(ri))
        interior = [vi for vi, rs in vert_regions.items() if len(rs) == 1]
        vg.add(interior, 1.0, "REPLACE")
        mod = obj.modifiers.new("RegionDecimate", "DECIMATE")
        mod.ratio = s.decimate_ratio
        mod.vertex_group = vg.name
        log("added Decimate modifier (unapplied), boundaries excluded from _RegionInterior")

    report = {
        "object": obj.name,
        "image": img.name,
        "analysis_size": [W, H],
        "triangles_before": tris_before,
        "triangles_after": tris_after,
        "new_vertices": len(new_verts),
        "weights_filled": filled,
        "merged_islands": merged,
        # color feature distance between every pair of groups: region_merge_distance above a pair's value joins them
        "color_distances": {"%s%d-%s%d" % (s.group_prefix, group_of[a], s.group_prefix, group_of[b]):
                            round(float(np.linalg.norm(centers[a] - centers[b])), 4)
                            for ai, a in enumerate(group_order) for b in group_order[ai + 1:]},
        "convention": s.convention,
        "shade_strength_for_material": s.shade_range,
        "groups": [dict({
            "group": "%s%d" % (s.group_prefix, group_of[c]),
            "faces": face_count[c],
            "srgb_hex": linear_to_hex(palette_lin[c]),
            "linear_rgb": [round(float(x), 5) for x in palette_lin[c]],
            "surface_share": round(float(area_share[c]), 4),
            "shade_range_needed": round(shade_needed[c], 3),
        }, **({"index": index_of[c], "custom_primitive_data_index": index_of[c] * 4,
               "region_id_value": table[index_of[c]]} if s.write_region_ids else {})) for c in group_order],
        "settings": asdict(s),
        "seconds": round(time.time() - _T0, 1),
    }
    text = bpy.data.texts.get("RegionReport") or bpy.data.texts.new("RegionReport")
    text.from_string(json.dumps(report, indent=2))
    log("triangles %d -> %d, %d new vertices (%d given neighbor weights)" % (tris_before, tris_after, len(new_verts), filled))
    if report_path:
        with open(report_path, "w") as fh:
            json.dump(report, fh, indent=2)
    if save_path:
        bpy.ops.wm.save_as_mainfile(filepath=save_path)
        log("saved", save_path)
    return obj, report


def _cli():
    s = SETTINGS
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser(prog="region_from_texture")
    as_bool = lambda x: str(x).lower() in ("1", "true", "yes", "on")
    for f in fields(Settings):
        p.add_argument("--" + f.name, type=as_bool if f.type is bool else f.type, default=getattr(s, f.name))
    p.add_argument("--save", default="")
    p.add_argument("--report", default="")
    a = p.parse_args(argv)
    s = Settings(**{f.name: getattr(a, f.name) for f in fields(Settings)})
    return s, a.save, a.report


if __name__ == "__main__":
    settings, save, report = _cli()
    run(settings, save, report)
