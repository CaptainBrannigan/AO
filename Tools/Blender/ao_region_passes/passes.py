"""
Color regions built pass by pass. Each pass saves its result, so any pass can be re-run on its own and you can see
exactly what it changed:

  1. Analyze   texture -> three maps, saved as images in the .blend (no mesh is touched):
               region map  every texel's region, drawn in the region's color (you can paint on it to fix a region)
               shade map   how much darker than its region's light tone each texel is (0 = light tone, 1 = darkest)
               fold map    depth per texel, grey 0.5 = none, darker = inward, lighter = outward (only with a depth source)
  2. Geometry  source mesh + region map -> a flat copy, `<source>_Passes`, cut along every region boundary.
               This is the only pass that places vertices: by splitting faces (subdivide) or tracing boundaries (targeted).
  3. Depth     moves `<source>_Passes`'s vertices along their normals by the fold map. Re-runnable: it always starts from
               the flat positions Geometry saved, so a new strength replaces the old one instead of stacking.
  4. Reduce    `<source>_Passes` -> `<source>_Passes_Reduced`, decimated to max_triangles. Re-runnable from the full mesh.

Region IDs then work on whichever result is active, as in the original add-on.

Uses its own copy of the engine (region_from_texture.py, targeted.py), so the original AO Color Regions add-on is unaffected.
"""

import dataclasses
import json
import time

import bmesh
import bpy
import mathutils
import mathutils.bvhtree
import mathutils.kdtree
import numpy as np

from . import region_from_texture as E

RESULT_SUFFIX = "_Passes"
REDUCED_SUFFIX = "_Passes_Reduced"

# which settings each pass reads (also decides when a saved pass is out of date)
PASS_FIELDS = {
    "analyze": ["image_name", "uv_layer", "region_count", "palette", "merge_distance", "auto_percentile", "auto_min_gain",
                "lightness_weight", "analysis_size", "cluster_samples", "kmeans_iterations", "kmeans_restarts", "seed",
                "majority_radius", "majority_iterations", "region_merge_distance", "shade_group_distance", "convention",
                "color_tolerance", "min_color_share", "darker_tolerance_stops", "lighter_tolerance_stops",
                "shade_range", "shade_reference_percentile", "index_remap",
                "depth_source", "depth_image", "depth_regions", "depth_curve", "depth_midlevel", "depth_blur_px",
                "depth_edge_falloff_px", "depth_threshold", "depth_preserve_volume", "depth_volume_radius_px",
                "depth_highpass_px", "normal_green"],
    "geometry": ["method", "targeted_area", "targeted_depth_scope", "targeted_tolerance_px", "targeted_depth_iterations",
                 "targeted_min_spacing_px", "targeted_keep_base_edges", "max_subdiv_levels", "min_edge_length",
                 "boundary_tolerance_px", "sample_spacing_px", "max_samples_per_edge", "boundary_search_steps",
                 "min_split_fraction", "seam_inset", "dissolve_unneeded", "depth_subdiv_levels", "depth_subdiv_threshold",
                 "min_region_area", "hide_source", "write_shade", "write_preview", "write_vertex_groups",
                 "group_prefix", "write_region_ids", "shade_edge_stops", "shade_edge_blur_px", "shade_edge_cleanup_px"],
    "depth": ["depth_strength", "depth_max", "depth_max_thickness", "depth_smooth_iterations"],
    "reduce": ["max_triangles", "reduce_protection_strength", "reduce_small_region_protection", "reduce_shade_tolerance",
               "reduce_keep_original_vertices", "reduce_shade_edge_weight"],
}


def log(*args):
    E.log(*args)


def subset(s, pass_name):
    return {k: getattr(s, k) for k in PASS_FIELDS[pass_name]}


def _record(obj, key):
    raw = obj.get(key) if obj is not None else None
    return json.loads(raw) if raw else None


# ----------------------------------------------------------------------------------------------
# Objects

def source_of(obj):
    """The textured source mesh for the active object (itself, or the source a result was made from)."""
    if obj is None:
        return None
    name = obj.get("aop_source")
    if name:
        return bpy.data.objects.get(name)
    return obj if obj.type == "MESH" else None


# the source object points at its current results (Object pointer properties registered in __init__), so renaming a
# result keeps it current and a copy of a result (Shift+D, which copies the tags too) is never mistaken for it
RESULT_POINTERS = {"full": "aop_result_full", "reduced": "aop_result_reduced"}


def _in_scene(o):
    return o is not None and len(o.users_collection) > 0


def _result_time(o, kind):
    rec = _record(o, "aop_geometry" if kind == "full" else "aop_reduce")
    return rec.get("time", 0.0) if rec else 0.0


def result_of(src, kind="full"):
    """The result the passes work on: the object the source points to (set whenever Geometry or Reduce makes one,
    and it follows renames). Without a pointer (files from 0.1.2 and earlier) the newest object tagged as this
    source's result of that kind, the one with the standard name winning a tie."""
    if src is None:
        return None
    prop = RESULT_POINTERS.get(kind)
    ptr = getattr(src, prop, None) if prop else None
    if _in_scene(ptr) and ptr.get("aop_kind") == kind and ptr.get("aop_source") == src.name:
        return ptr
    standard = src.name + (REDUCED_SUFFIX if kind == "reduced" else RESULT_SUFFIX)
    found = [o for o in bpy.data.objects
             if o.get("aop_source") == src.name and o.get("aop_kind") == kind and _in_scene(o)]
    if not found:
        return None
    return max(found, key=lambda o: (_result_time(o, kind), o.name == standard))


def is_snapshot(obj):
    """A copy of a result (tagged like one, but not the result its source points to)."""
    src = source_of(obj)
    kind = obj.get("aop_kind") if obj is not None else None
    if src is None or kind not in RESULT_POINTERS:
        return False
    current = result_of(src, kind)
    return current is not None and current is not obj


def _replace_result(src, kind, base, name):
    """A fresh copy of `base` as this pass's result. The previous result of the same kind goes only while it still has
    the standard name; a result you renamed is kept (as a copy), so renaming is how you keep one."""
    old = result_of(src, kind)
    if old is not None and old is not base and (old.name == name or old.name.startswith(name + ".")):
        me = old.data
        bpy.data.objects.remove(old)
        if me.users == 0:
            bpy.data.meshes.remove(me)
    elif old is not None:
        log("keeping %s: it was renamed, so it stays as a copy" % old.name)
    obj = base.copy()
    obj.data = base.data.copy()
    obj.name = name
    obj.data.name = name
    for k in ("aop_analysis", "aop_geometry", "aop_depth", "aop_reduce"):
        if k in obj:
            del obj[k]
    obj["aop_source"] = src.name
    obj["aop_kind"] = kind
    for col in base.users_collection:
        col.objects.link(obj)
    prop = RESULT_POINTERS.get(kind)
    if prop and hasattr(src, prop):
        setattr(src, prop, obj)
    return obj


# ----------------------------------------------------------------------------------------------
# Saved maps

def _map_image(name, w, h, colorspace):
    img = bpy.data.images.get(name)
    if img is not None and tuple(img.size) != (w, h):
        bpy.data.images.remove(img)
        img = None
    if img is None:
        img = bpy.data.images.new(name, w, h, alpha=False)
    img.colorspace_settings.name = colorspace
    img.use_fake_user = True            # no material uses the maps; without this Blender drops them on save
    return img


def _write_image(img, rgb):
    h, w = rgb.shape[:2]
    rgba = np.concatenate([np.clip(rgb, 0.0, 1.0), np.ones((h, w, 1), np.float32)], -1).astype(np.float32)
    img.pixels.foreach_set(rgba.ravel())
    img.update()
    img.pack()                          # saved inside the .blend


def load_maps(src):
    """Region labels, shade and fold arrays from the saved images (any painting on the region map is picked up)."""
    meta = _record(src, "aop_analysis")
    if meta is None:
        raise RuntimeError("Run 1. Analyze on %s first" % src.name)
    for key in ("region_map", "shade_map"):
        im = bpy.data.images.get(meta[key])
        if im is None or im.size[0] == 0:
            raise RuntimeError("The %s %s is missing%s; run 1. Analyze again" % (
                key.replace("_", " "), meta[key], " (its file %s isn't there)" % im.filepath if im is not None else ""))
    reg = bpy.data.images.get(meta["region_map"])
    px = np.round(E.image_pixels(reg)[..., :3] * 255.0).astype(np.float32)
    pal = np.array(meta["palette_srgb8"], dtype=np.float32)
    flat = px.reshape(-1, 3)
    labels = np.empty(len(flat), dtype=np.int8)
    for i in range(0, len(flat), 1 << 20):
        d = ((flat[i:i + (1 << 20), None, :] - pal[None, :, :]) ** 2).sum(-1)
        labels[i:i + (1 << 20)] = np.argmin(d, axis=1)
    labels = labels.reshape(px.shape[:2])
    shade = E.image_pixels(bpy.data.images[meta["shade_map"]])[..., 0].astype(np.float32)
    fold = None
    if meta.get("fold_map"):
        fold = (E.image_pixels(bpy.data.images[meta["fold_map"]])[..., 0].astype(np.float64) - 0.5) * 2.0
    return meta, labels, shade, fold


def _box_count(a, r):
    """Box sum of radius r of an integer array, separable with int32 running sums (exact at any map size)."""
    n = 2 * r + 1
    for ax in (0, 1):
        a = np.moveaxis(a, ax, 0)
        p = np.pad(a, ((r, r), (0, 0)), mode="edge")
        c = np.concatenate([np.zeros((1,) + p.shape[1:], np.int32), np.cumsum(p, axis=0, dtype=np.int32)])
        a = np.moveaxis(c[n:] - c[:-n], 0, ax)
    return a


def _majority(lab, r, iterations=2):
    """Each texel takes the most common label within radius r (ties keep the earlier label); patches narrower than
    about 2r disappear into their surroundings."""
    for _ in range(iterations):
        best = np.full(lab.shape, -1, np.int32)
        out = lab.copy()
        for j in np.unique(lab):
            c = _box_count((lab == j).astype(np.int32), r)
            m = c > best
            best[m] = c[m]
            out[m] = j
        lab = out
    return lab


def shade_steps(labels, shade, shade_range, s):
    """Shade step per texel for edges inside regions: 0 = the region's light tone, then a new step every
    shade_edge_stops of darkening. None when shade_edge_stops is 0. The shade map is smoothed within each region
    first (shade_edge_blur_px), so a dark neighbour doesn't leave a strip of darker steps along every border."""
    if s.shade_edge_stops <= 0:
        return None
    sh = shade.astype(np.float64)
    r = s.shade_edge_blur_px
    if r > 0:
        out = np.zeros_like(sh)
        for c in np.unique(labels):
            m = (labels == c).astype(np.float64)
            out += np.where(m > 0, E.box_blur(sh * m, r) / np.maximum(E.box_blur(m, r), 1e-6), 0.0)
        sh = out
    stops = -np.log2(np.clip(1.0 - sh * shade_range, 1e-4, 1.0))
    steps = np.minimum(np.floor(stops / s.shade_edge_stops), 255).astype(np.int32)
    if s.shade_edge_cleanup_px > 0:
        steps = _majority(steps, s.shade_edge_cleanup_px)
    return steps


def edge_map(labels, steps):
    """What Geometry cuts along: region borders, plus shade-step borders inside each region."""
    if steps is None:
        return E.Map(labels)
    return E.Map(labels.astype(np.int32) * (int(steps.max()) + 1) + steps)


def preview_shade_edges(src, s):
    """Saves AOP_<src>_shade_edges: each region's color darkened to the middle of each shade step, i.e. the flat
    colors the mesh will show between its edges. Uses the current Geometry settings; no mesh is touched."""
    meta, labels, shade, _ = load_maps(src)
    rng = meta["settings"].get("shade_range", s.shade_range)
    steps = shade_steps(labels, shade, rng, s)
    pal = np.array(meta["palette_lin"], dtype=np.float32)
    if steps is None:
        lin = pal[labels]
        n_steps = 1
    else:
        lin = pal[labels] * (2.0 ** (-(steps + 0.5) * s.shade_edge_stops))[..., None].astype(np.float32)
        n_steps = int(steps.max()) + 1
    e = edge_map(labels, steps).a
    edge_px = int(((e[:, 1:] != e[:, :-1]).sum() + (e[1:, :] != e[:-1, :]).sum()))
    H, W = labels.shape
    img = _map_image("AOP_%s_shade_edges" % src.name, W, H, "sRGB")
    _write_image(img, np.clip(E.linear_to_srgb(lin), 0, 1).astype(np.float32))
    return img, n_steps, edge_px


def disp_map(fold, s):
    """World-unit displacement from the saved fold map and the current Depth settings."""
    if fold is None:
        return None
    return E.Map(np.clip(-fold * s.depth_strength, -s.depth_max, s.depth_max))


# ----------------------------------------------------------------------------------------------
# 1. Analyze

def analyze(src, s):
    E._T0 = time.time()
    img = bpy.data.images.get(s.image_name) if s.image_name else E.find_material_image(src, "color")
    if img is None:
        raise RuntimeError("No color texture found (set the Texture field)")
    uv_name = s.uv_layer or src.data.uv_layers.active.name
    log("analyze %s: image %s %s, uv %s" % (src.name, img.name, tuple(img.size), uv_name))

    px = E.image_pixels(img)[..., :3]
    lin = px if img.is_float else E.srgb_to_linear(px)
    lin = E.downsample(lin, s.analysis_size).astype(np.float32)
    H, W = lin.shape[:2]
    feat_full = E.color_features(lin.reshape(-1, 3), s.lightness_weight)
    color_map = E.Map(lin)

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
    sample_feat = E.color_features(sample_lin, s.lightness_weight)

    max_regions = len(E.region_value_table(s.convention))
    if s.palette:
        centers = np.array([E.color_features(E.hex_to_linear(h), s.lightness_weight) for h in s.palette.split(",")])
    elif s.region_count > 0:
        centers = E.kmeans(sample_feat, min(s.region_count, max_regions), s.kmeans_iterations, rng, s.kmeans_restarts)
    else:
        trials = []
        for k in range(1, max_regions + 1):
            trial = E.kmeans(sample_feat, k, s.kmeans_iterations, rng, s.kmeans_restarts)
            _, dist = E.assign(sample_feat, trial)
            spread = float(np.percentile(dist, s.auto_percentile))
            log("auto: %d regions -> %.1f%% of surface within %.3f" % (k, s.auto_percentile, spread))
            trials.append((trial, spread))
            if spread <= s.merge_distance:
                break
        centers = trials[-1][0]
        if trials[-1][1] > s.merge_distance:
            floor = min(t[1] for t in trials)
            k_pick = next(i for i, t in enumerate(trials) if t[1] <= floor * (1.0 + s.auto_min_gain))
            centers = trials[k_pick][0]
            log("auto: merge_distance %.3f is below the floor %.3f; using %d regions" % (s.merge_distance, floor, k_pick + 1))
    if not s.palette and (s.color_tolerance > 0 or s.darker_tolerance_stops > 0 or s.lighter_tolerance_stops > 0):
        sample_solid = E.Map(E.solid_map(feat_full.reshape(H, W, 3), max(s.color_tolerance, 0.05))).nearest(sample_uv)
        centers = E.add_tolerance_regions(centers, sample_feat, E.luminance(sample_lin), sample_solid, s, max_regions, rng)
    centers, joined = E.merge_close_centers(sample_feat, centers, s.region_merge_distance)
    if joined:
        log("region_merge_distance joined %d region colors" % joined)
    k = len(centers)
    if k > max_regions:
        raise RuntimeError("%d regions exceed the %s convention's %d" % (k, s.convention, max_regions))

    labels, _ = E.assign(feat_full, centers.astype(np.float32))
    group_of = E.shade_groups(centers, sample_feat, sample_lin, s)
    if group_of is not None and group_of.max() + 1 < k:
        n_groups = int(group_of.max()) + 1
        labels = group_of[labels]
        sl, _ = E.assign(sample_feat, centers.astype(np.float32))
        sg = group_of[sl]
        centers = np.array([sample_feat[sg == j].mean(0) for j in range(n_groups)], dtype=np.float32)
        log("shade_group_distance grouped %d region colors into %d regions" % (k, n_groups))
        k = n_groups
    labels = labels.reshape(H, W).astype(np.int8)
    labels = E.majority_filter(labels, k, s.majority_radius, s.majority_iterations)
    labelmap = E.Map(labels)

    sample_lab = labelmap.nearest(sample_uv)
    area_share = np.bincount(sample_lab, minlength=k) / float(len(sample_lab))
    order = list(np.argsort(-area_share))
    index_of = {int(c): i for i, c in enumerate(order)}
    if s.index_remap:
        remap = dict(tuple(int(x) for x in p.split(":")) for p in s.index_remap.split(","))
        index_of = {c: remap.get(i, i) for c, i in index_of.items()}

    Y = E.luminance(lin)
    sample_Y = E.luminance(sample_lin)
    y_ref = np.array([np.percentile(sample_Y[sample_lab == c], s.shade_reference_percentile) if np.any(sample_lab == c) else 1.0
                      for c in range(k)])
    shade = np.clip((y_ref[labels] - Y) / np.maximum(y_ref[labels] * s.shade_range, 1e-6), 0.0, 1.0).astype(np.float32)
    sample_shade = E.Map(shade).nearest(sample_uv)
    shade_needed = [float(np.clip(1 - np.percentile(sample_Y[sample_lab == c], 5) / max(y_ref[c], 1e-6), 0, 1))
                    if np.any(sample_lab == c) else 0.0 for c in range(k)]
    palette_lin = []
    for c in range(k):
        mask = (sample_lab == c) & (sample_shade <= 0.1)
        if not np.any(mask):
            mask = sample_lab == c
        palette_lin.append(sample_lin[mask].mean(0) if np.any(mask) else np.zeros(3))

    fold = None
    if s.depth_source != "none":
        if s.depth_source == "luminance":
            fold = shade.astype(np.float64)
        else:
            dimg = bpy.data.images.get(s.depth_image) if s.depth_image else E.find_material_image(src, "normal")
            if dimg is None:
                raise RuntimeError("depth_source '%s' needs a depth image" % s.depth_source)
            raw = E.downsample(E.image_pixels(dimg)[..., :3], max(s.analysis_size, 1))
            if s.depth_source == "height":
                h = E.luminance(raw) - s.depth_midlevel
                fold = -np.clip(h / max(s.depth_midlevel, 1e-6), -1, 1)
            else:
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
                z = z - E.box_blur(z, s.depth_highpass_px)
                fold = -z / max(np.percentile(np.abs(z), 99), 1e-6)
            fold = E.resample_to(np.clip(fold, -1, 1), H, W)
        if s.depth_threshold > 0:
            t = min(s.depth_threshold, 0.99)
            fold = np.sign(fold) * np.clip((np.abs(fold) - t) / (1.0 - t), 0.0, 1.0)
        fold = np.sign(fold) * np.abs(fold) ** s.depth_curve
        fold = E.box_blur(fold.astype(np.float64), s.depth_blur_px)
        hard = np.ones(fold.shape, dtype=np.float64)
        if s.depth_regions:
            hard = np.isin(labels, E.depth_clusters(s.depth_regions, palette_lin, index_of)).astype(np.float64)
            fold = fold * E.box_blur(hard, max(s.depth_edge_falloff_px, 1)) * hard
        if s.depth_preserve_volume > 0:
            r = max(s.depth_volume_radius_px, 1)
            local = E.box_blur(fold, r, passes=1) / np.maximum(E.box_blur(hard, r, passes=1), 1e-6)
            fold = fold - s.depth_preserve_volume * local * hard
        fold = np.clip(fold, -1.0, 1.0)

    # save the maps as images
    pal_srgb = np.clip(E.linear_to_srgb(np.array(palette_lin)), 0, 1)
    pal8 = np.round(pal_srgb * 255.0)
    base = "AOP_%s_" % src.name
    reg = _map_image(base + "regions", W, H, "sRGB")
    _write_image(reg, (pal8 / 255.0)[labels].astype(np.float32))
    sh = _map_image(base + "shade", W, H, "Non-Color")
    _write_image(sh, np.repeat(shade[..., None], 3, axis=-1))
    fold_name = ""
    if fold is not None:
        fm = _map_image(base + "fold", W, H, "Non-Color")
        _write_image(fm, np.repeat((0.5 + 0.5 * fold)[..., None], 3, axis=-1).astype(np.float32))
        fold_name = fm.name
    elif bpy.data.images.get(base + "fold") is not None:
        bpy.data.images.remove(bpy.data.images[base + "fold"])

    meta = {
        "time": time.time(), "image": img.name, "uv": uv_name, "W": W, "H": H, "k": k,
        "palette_lin": [[float(x) for x in p] for p in palette_lin], "palette_srgb8": pal8.astype(int).tolist(),
        "centers": np.asarray(centers, dtype=np.float64).tolist(), "area_share": area_share.tolist(),
        "index_of": {str(c): int(i) for c, i in index_of.items()}, "shade_needed": shade_needed,
        "region_map": reg.name, "shade_map": sh.name, "fold_map": fold_name, "settings": subset(s, "analyze"),
        "seconds": round(time.time() - E._T0, 1),
    }
    src["aop_analysis"] = json.dumps(meta)
    log("analyze: %d regions, maps %dx%d saved (%s%s)" % (k, W, H, reg.name, ", " + fold_name if fold_name else ""))
    return meta


# ----------------------------------------------------------------------------------------------
# 2. Geometry

def geometry(src, s):
    E._T0 = time.time()
    meta, labels, shade, fold = load_maps(src)
    k = meta["k"]
    uv_name = meta["uv"]
    labelmap, shade_map = E.Map(labels), E.Map(shade)
    shade_range = meta["settings"].get("shade_range", s.shade_range)
    steps = shade_steps(labels, shade, shade_range, s)
    cutmap = edge_map(labels, steps)             # region borders, plus shade-step borders when shade_edge_stops > 0
    if steps is not None:
        log("geometry: shade edges every %.2f stops, %d steps" % (s.shade_edge_stops, int(steps.max()) + 1))
    dispmap = disp_map(fold, s)
    centers = np.array(meta["centers"], dtype=np.float32)
    palette_lin = [np.array(p) for p in meta["palette_lin"]]
    area_share = np.array(meta["area_share"])
    index_of = {int(c): i for c, i in meta["index_of"].items()}
    m3 = np.array(src.matrix_world.to_3x3(), dtype=np.float64)

    obj = _replace_result(src, "full", src, src.name + RESULT_SUFFIX)
    if s.hide_source:
        src.hide_set(True)
    me = obj.data
    for name in ("RegionID", "Shade", "RegionPreview", "RegionIndex", "RegionGroup", "RegionGroupMask",
                 "_rft_orig", "_rft_island", "aop_rest", "ShadeStep"):
        a = me.attributes.get(name)
        if a:
            me.attributes.remove(a)
    for vg in list(obj.vertex_groups):
        if E.is_region_group(vg.name, s.group_prefix):
            obj.vertex_groups.remove(vg)

    bm = bmesh.new()
    bm.from_mesh(me)
    uvl = bm.loops.layers.uv[uv_name]
    tris_before = sum(len(f.verts) - 2 for f in bm.faces)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3], quad_method="BEAUTY", ngon_method="BEAUTY")
    bm.verts.index_update()
    kd = mathutils.kdtree.KDTree(len(bm.verts))
    for v in bm.verts:
        kd.insert(v.co, v.index)
    kd.balance()
    dims = [max(v.co[i] for v in bm.verts) - min(v.co[i] for v in bm.verts) for i in range(3)]
    same_eps = max(max(dims), 1e-6) * 1e-7
    is_original = lambda v: kd.find(v.co)[2] <= same_eps

    if s.method == "targeted":
        E._targeted.targeted_inject(bm, uvl, cutmap, dispmap, s, E.faces_needing_refinement, E.tri_arrays, log)
        E.triangulate_non_tris(bm)
    else:
        E.refine(bm, uvl, m3, cutmap, dispmap, s, E.REFINE_SAFETY_TRIANGLES)
        E.cut_boundaries(bm, uvl, cutmap, s)
    log("geometry: %d triangles after %s" % (len(bm.faces), "injecting islands" if s.method == "targeted" else "refinement and the cut"))
    lab_layer = bm.faces.layers.int.new("_rft_label")
    E.label_faces(bm, uvl, labelmap, lab_layer)
    distinct = []
    if s.color_tolerance > 0 and k > 1:
        cd = np.sqrt(((centers[:, None, :] - centers[None, :, :]) ** 2).sum(-1))
        np.fill_diagonal(cd, np.inf)
        distinct = [c for c in range(k) if cd[c].min() > s.color_tolerance]
    merged = E.merge_small_regions(bm, lab_layer, m3, s.min_region_area, distinct, s.min_color_share)
    step_layer = None
    keep_layer = lab_layer
    if steps is not None:
        # each face's shade step (written as the ShadeStep face attribute; Reduce keeps the edges between steps)
        step_layer = bm.faces.layers.int.new("ShadeStep")
        E.label_faces(bm, uvl, E.Map(steps), step_layer)
        keep_layer = bm.faces.layers.int.new("_aop_cut")
        n_steps = int(steps.max()) + 1
        for f in bm.faces:
            f[keep_layer] = f[lab_layer] * n_steps + f[step_layer]
    if s.dissolve_unneeded and s.method != "targeted":
        E.dissolve_unneeded(bm, uvl, keep_layer, is_original, dispmap, s)
    if keep_layer is not lab_layer:
        bm.faces.layers.int.remove(keep_layer)
    new_verts = [v for v in bm.verts if not is_original(v)]
    E.fill_missing_weights(bm, new_verts)

    face_count = {}
    for f in bm.faces:
        face_count[f[lab_layer]] = face_count.get(f[lab_layer], 0) + 1
    group_order = sorted(face_count, key=lambda c: -face_count[c])
    group_of = {int(c): g + 1 for g, c in enumerate(group_order)}

    table = E.region_value_table(s.convention, E.precompensated(s))
    grp_layer = bm.faces.layers.int.new("RegionGroup")
    idx_layer = bm.faces.layers.int.new("RegionIndex") if s.write_region_ids else None
    reg_layer = bm.loops.layers.float_color.new("RegionID") if s.write_region_ids else None
    shade_layer = bm.loops.layers.float_color.new("Shade") if s.write_shade else None
    prev_layer = bm.loops.layers.float_color.new("RegionPreview") if s.write_preview else None
    loops, uvs, cens, face_step = [], [], [], []
    for f in bm.faces:
        c = f[lab_layer]
        f[grp_layer] = group_of[c]
        if reg_layer:
            f[idx_layer] = index_of[c]
            r = table[min(index_of[c], len(table) - 1)]
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
            if step_layer is not None:
                face_step.append(f[step_layer])
    if shade_layer:
        sv = shade_map.nearest(E.inset(np.array(uvs), np.array(cens), s.seam_inset))
        if step_layer is not None:
            # every corner stays within its face's step, so the step reads as a crisp change at the shared edge
            # (a corner right on the edge could otherwise sample the texel of the step next door)
            fs = np.array(face_step, dtype=np.float64)
            to_shade = lambda st: (1.0 - 2.0 ** (-st * s.shade_edge_stops)) / max(shade_range, 1e-6)
            sv = np.clip(sv, to_shade(fs), np.minimum(to_shade(fs + 1), 1.0)).astype(np.float32)
        if E.precompensated(s):
            sv = E.linear_to_srgb(sv)
        for l, v in zip(loops, sv):
            l[shade_layer] = (float(v), float(v), float(v), 1.0)
    bm.faces.layers.int.remove(lab_layer)
    orig_layer = bm.verts.layers.int.new("_rft_orig")
    for v in bm.verts:
        v[orig_layer] = 1 if is_original(v) else 0

    bm.to_mesh(me)
    bm.free()
    me[E.ENCODING_KEY] = s.ue_mesh_type
    me.update()
    rest = np.empty(len(me.vertices) * 3, dtype=np.float32)
    me.vertices.foreach_get("co", rest)
    me.attributes.new("aop_rest", "FLOAT_VECTOR", "POINT").data.foreach_set("vector", rest)
    E.set_color_attributes(me, "RegionPreview" if s.write_preview else None, "RegionID" if s.write_region_ids else None)
    if s.write_vertex_groups:
        E.write_region_groups(obj, s.group_prefix)
    rec = {"time": time.time(), "analysis_time": meta["time"], "settings": subset(s, "geometry"),
           "depth_strength_used": s.depth_strength, "triangles": len(me.polygons), "triangles_before": tris_before,
           "new_vertices": len(new_verts), "merged_islands": merged,
           "shade_steps": 0 if steps is None else int(steps.max()) + 1, "seconds": round(time.time() - E._T0, 1),
           "groups": [{"group": "%s%d" % (s.group_prefix, group_of[c]), "faces": face_count[c],
                       "srgb_hex": E.linear_to_hex(palette_lin[c]), "surface_share": round(float(area_share[c]), 4)}
                      for c in group_order]}
    obj["aop_geometry"] = json.dumps(rec)
    log("geometry: %s, %d -> %d triangles, %d regions" % (obj.name, tris_before, rec["triangles"], len(group_order)))
    return obj, rec


# ----------------------------------------------------------------------------------------------
# 3. Depth

def depth(src, s):
    E._T0 = time.time()
    obj = result_of(src, "full")
    if obj is None:
        raise RuntimeError("Run 2. Geometry first")
    meta, labels, shade, fold = load_maps(src)
    me = obj.data
    if "aop_rest" not in me.attributes:
        raise RuntimeError("%s has no saved flat positions; run 2. Geometry again" % obj.name)
    rest = np.empty(len(me.vertices) * 3, dtype=np.float32)
    me.attributes["aop_rest"].data.foreach_get("vector", rest)
    rest = rest.reshape(-1, 3)
    geo = _record(obj, "aop_geometry") or {}
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.index_update()
    for v in bm.verts:
        v.co = rest[v.index]
    moved = limited = 0
    dispmap = disp_map(fold, s)
    if dispmap is not None and s.depth_strength > 0:
        uvl = bm.loops.layers.uv[meta["uv"]]
        m3 = np.array(obj.matrix_world.to_3x3(), dtype=np.float64)
        d = E.vertex_displacement(bm, uvl, dispmap, s)
        bm.normal_update()
        dims = np.ptp(rest, axis=0)
        eps = max(float(dims.max()), 1e-6) * 1e-4
        bvh = mathutils.bvhtree.BVHTree.FromBMesh(bm) if s.depth_max_thickness < 1.0 else None
        isl = bm.verts.layers.int.get("_rft_island")
        moves = []
        for v in bm.verts:
            dv = d[v.index]
            if isl is not None and v[isl] != 1:
                continue                                    # targeted: only island interiors move
            if abs(dv) < 1e-7:
                continue
            n = v.normal.copy()
            scale = np.linalg.norm(m3 @ np.array(n[:]))
            amount = dv / max(scale, 1e-9)
            if bvh is not None and amount < 0:
                hit = bvh.ray_cast(v.co - n * eps, -n)
                if hit[0] is not None:
                    lim = s.depth_max_thickness * (hit[3] + eps)
                    if -amount > lim:
                        amount = -lim
                        limited += 1
            moves.append((v, n * amount))
        for v, off in moves:
            v.co += off
        moved = len(moves)
    bm.to_mesh(me)
    bm.free()
    me.update()
    rec = {"time": time.time(), "geometry_time": geo.get("time"), "settings": subset(s, "depth"), "moved": moved,
           "limited": limited, "flat": dispmap is None or s.depth_strength <= 0, "seconds": round(time.time() - E._T0, 1)}
    obj["aop_depth"] = json.dumps(rec)
    log("depth: %s, %d vertices moved, %d limited by thickness" % (obj.name, moved, limited))
    return obj, rec


def flatten(src):
    obj = result_of(src, "full")
    if obj is None or "aop_rest" not in obj.data.attributes:
        raise RuntimeError("Run 2. Geometry first")
    me = obj.data
    rest = np.empty(len(me.vertices) * 3, dtype=np.float32)
    me.attributes["aop_rest"].data.foreach_get("vector", rest)
    me.vertices.foreach_set("co", rest)
    me.update()
    if "aop_depth" in obj:
        del obj["aop_depth"]
    return obj


# ----------------------------------------------------------------------------------------------
# 4. Reduce

def reduce(src, s):
    E._T0 = time.time()
    full = result_of(src, "full")
    if full is None:
        raise RuntimeError("Run 2. Geometry first")
    obj = _replace_result(src, "reduced", full, src.name + REDUCED_SUFFIX)
    full.hide_set(True)
    me = obj.data
    if "aop_rest" in me.attributes:
        me.attributes.remove(me.attributes["aop_rest"])
    bm = bmesh.new()
    bm.from_mesh(me)
    start = len(bm.faces)
    # Shade is read back for the reduction weights in the encoding the mesh actually has
    s_mesh = dataclasses.replace(s, ue_mesh_type=E.mesh_encoding(full.data))
    bm = E.reduce_to_budget(bm, s_mesh, [vg.name for vg in obj.vertex_groups])   # in index order: bone weights stay lined up
    bm.to_mesh(me)
    bm.free()
    me.update()
    if s.write_vertex_groups:
        E.write_region_groups(obj, s.group_prefix)
    geo, dep = _record(full, "aop_geometry") or {}, _record(full, "aop_depth") or {}
    rec = {"time": time.time(), "geometry_time": geo.get("time"), "depth_time": dep.get("time"),
           "settings": subset(s, "reduce"), "triangles_before": start, "triangles": len(me.polygons),
           "seconds": round(time.time() - E._T0, 1)}
    obj["aop_reduce"] = json.dumps(rec)
    log("reduce: %s, %d -> %d triangles" % (obj.name, start, rec["triangles"]))
    return obj, rec


# ----------------------------------------------------------------------------------------------
# Pack for export

EXPORT_SUFFIX = "_Export"
PACKED_NAME = "VertexColorPacked"


def pack_for_export(obj, s):
    """Copies a finished result to `<name>_Export` holding a single Face Corner color attribute, VertexColorPacked:
    R = RegionID, G = 1 (AO, unused for now), B = Shade, A = 1. UE imports only one vertex color set, so the copy keeps
    nothing else. The values are copied as written (already encoded for the mesh's UE type); the result is not changed."""
    if obj is None or obj.type != "MESH":
        raise RuntimeError("Select the finished mesh (the Geometry or Reduce result)")
    if obj.get("aop_kind") == "export":
        raise RuntimeError("%s is already an export copy; select the mesh it was made from" % obj.name)
    me0 = obj.data
    if "RegionID" not in me0.color_attributes:
        raise RuntimeError("%s has no RegionID: assign the groups' IDs and Apply IDs first" % obj.name)
    written = E.mesh_encoding(me0)
    if written != s.ue_mesh_type:
        raise RuntimeError("%s is written for a %s mesh but Import in UE as is %s: Apply IDs to re-encode it first"
                           % (obj.name, written, s.ue_mesh_type))
    for name in ("RegionID", "Shade"):
        a = me0.color_attributes.get(name)
        if a is not None and a.domain != "CORNER":
            raise RuntimeError("%s on %s must be Face Corner, not %s" % (name, obj.name, a.domain))

    name = obj.name + EXPORT_SUFFIX
    old = bpy.data.objects.get(name)
    if old is not None:
        old_me = old.data
        bpy.data.objects.remove(old)
        if old_me is not None and old_me.users == 0:
            bpy.data.meshes.remove(old_me)
    exp = obj.copy()
    exp.data = me0.copy()
    exp.name = name
    exp.data.name = name
    for k in ("aop_analysis", "aop_geometry", "aop_depth", "aop_reduce"):
        if k in exp:
            del exp[k]
    exp["aop_source"] = obj.get("aop_source", obj.name)
    exp["aop_kind"] = "export"          # never mistaken for the Geometry or Reduce result
    for col in obj.users_collection:
        col.objects.link(exp)

    me = exp.data
    n = len(me.loops)

    def corner(attr):
        c = np.empty(n * 4, dtype=np.float32)
        me.color_attributes[attr].data.foreach_get("color", c)
        return c.reshape(-1, 4)

    packed = np.ones((n, 4), dtype=np.float32)
    packed[:, 0] = corner("RegionID")[:, 0]
    has_shade = "Shade" in me.color_attributes
    packed[:, 2] = corner("Shade")[:, 0] if has_shade else 0.0
    # removed by name, one lookup at a time: removing an attribute invalidates Python references to the others
    for attr in [a.name for a in me.color_attributes]:
        a = me.color_attributes.get(attr)
        if a is not None:
            me.color_attributes.remove(a)
    me.color_attributes.new(PACKED_NAME, "FLOAT_COLOR", "CORNER").data.foreach_set("color", packed.ravel())
    left = [a.name for a in me.color_attributes]
    if left != [PACKED_NAME]:
        raise RuntimeError("%s still has color attributes %s" % (exp.name, left))
    me.color_attributes.active_color = me.color_attributes[PACKED_NAME]
    me.color_attributes.render_color_index = 0
    me.update()
    ids = sorted(set(np.round(packed[:, 0], 4).tolist()))
    log("pack: %s, %d corners, %s mesh, RegionID values %s%s" % (exp.name, n, written, ids, "" if has_shade else ", no Shade (B = 0)"))
    return exp, {"corners": n, "encoding": written, "region_values": ids, "shade": has_shade}


# ----------------------------------------------------------------------------------------------
# Pass status for the panel

def status(src, s):
    """Per pass: (state, text). state: 'none' (not run), 'done', 'stale' (its settings or an earlier pass changed)."""
    out = {}
    meta = _record(src, "aop_analysis")
    if meta is None:
        out["analyze"] = ("none", "Not run")
    else:
        changed = [k for k, v in subset(s, "analyze").items() if meta["settings"].get(k) != v]
        out["analyze"] = ("stale" if changed else "done",
                          "%d regions, maps %dx%d" % (meta["k"], meta["W"], meta["H"]) +
                          (" | changed: %s" % ", ".join(changed[:3]) if changed else ""))
    full = result_of(src, "full")
    geo = _record(full, "aop_geometry")
    if geo is None:
        out["geometry"] = ("none", "Not run")
    else:
        changed = [k for k, v in subset(s, "geometry").items() if geo["settings"].get(k) != v]
        upstream = meta is None or geo["analysis_time"] != meta["time"]
        out["geometry"] = ("stale" if changed or upstream else "done",
                           "%s: %d tris, %d regions" % (full.name, geo["triangles"], len(geo["groups"])) +
                           (", %d shade steps" % geo["shade_steps"] if geo.get("shade_steps") else "") +
                           (" | Analyze re-run since" if upstream else "") +
                           (" | changed: %s" % ", ".join(changed[:3]) if changed else ""))
    dep = _record(full, "aop_depth")
    if dep is None:
        out["depth"] = ("none", "Flat: depth not applied" if geo else "Not run")
    else:
        changed = [k for k, v in subset(s, "depth").items() if dep["settings"].get(k) != v]
        upstream = geo is None or dep["geometry_time"] != geo["time"]
        out["depth"] = ("stale" if changed or upstream else "done",
                        ("%d vertices moved" % dep["moved"] if not dep["flat"] else "Flat") +
                        (" | Geometry re-run since" if upstream else "") +
                        (" | changed: %s" % ", ".join(changed[:3]) if changed else ""))
    red = result_of(src, "reduced")
    rr = _record(red, "aop_reduce")
    if rr is None:
        out["reduce"] = ("none", "Not run")
    else:
        changed = [k for k, v in subset(s, "reduce").items() if rr["settings"].get(k) != v]
        upstream = geo is None or rr["geometry_time"] != geo["time"] or rr["depth_time"] != (dep or {}).get("time")
        out["reduce"] = ("stale" if changed or upstream else "done",
                         "%s: %d -> %d tris" % (red.name, rr["triangles_before"], rr["triangles"]) +
                         (" | earlier pass re-run since" if upstream else "") +
                         (" | changed: %s" % ", ".join(changed[:3]) if changed else ""))
    return out
