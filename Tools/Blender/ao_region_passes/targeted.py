"""
Targeted detail: keep the base mesh and inject detailed islands only where the texture needs them.

  1. Prepass, color borders: base faces a color boundary crosses (sampled about once per texel).
  2. Prepass, height: base faces whose flat surface misses the fold depth by more than depth_subdiv_threshold.
  3. Islands: connected detail faces inside one UV island (or the source mesh's selected faces).
  4. Injection, per island: trace the color boundaries in the region map, simplify them to targeted_tolerance_px,
     rebuild the island with a constrained Delaunay triangulation (boundaries and base edges as constraints, so every
     new triangle stays inside one region and inside one base triangle: the surface is unchanged), then add height
     points where folds still need them. New vertices take position, bone weights and UVs from the base triangle they
     lie in. Island borders keep their edges; where a new point lands on one, the edge is split on the outside face too.

Used by region_from_texture.run() when Settings.method == "targeted". bmesh.ops is not used until all islands are
done, because BMesh element wrappers don't keep their identity across bmesh.ops calls.
"""

import math

import bmesh
import numpy as np
from mathutils import Vector
from mathutils import geometry as mgeo


# ----------------------------------------------------------------------------------------------
# Region map contours

def label_contours(L, inside):
    """Boundaries between different labels as chains of texel corners (x, y) in crop coordinates.
    Only texel edges with both neighbouring texels inside the island count. Chains break where three labels meet."""
    adj = {}

    def add(a, b):
        adj.setdefault(a, []).append(b)
        adj.setdefault(b, []).append(a)

    dv = (L[:, 1:] != L[:, :-1]) & inside[:, 1:] & inside[:, :-1]
    for y, x in zip(*np.nonzero(dv)):
        add((x + 1, y), (x + 1, y + 1))
    dh = (L[1:, :] != L[:-1, :]) & inside[1:, :] & inside[:-1, :]
    for y, x in zip(*np.nonzero(dh)):
        add((x, y + 1), (x + 1, y + 1))
    used = set()
    chains = []

    def walk(start, nxt):
        chain = [start, nxt]
        used.add(frozenset((start, nxt)))
        prev, cur = start, nxt
        while len(adj[cur]) == 2 and cur != start:
            a, b = adj[cur]
            n = b if a == prev else a
            e = frozenset((cur, n))
            if e in used:
                break
            used.add(e)
            chain.append(n)
            prev, cur = cur, n
        return chain

    for node, nbs in adj.items():
        if len(nbs) != 2:
            for n in nbs:
                if frozenset((node, n)) not in used:
                    chains.append(walk(node, n))
    for node, nbs in adj.items():                         # closed loops with no junction
        for n in nbs:
            if frozenset((node, n)) not in used:
                chains.append(walk(node, n))
    return chains


def simplify(points, tol):
    """Douglas-Peucker on a chain of 2D points (closed chains keep start = end)."""
    P = np.asarray(points, dtype=np.float64)
    if len(P) <= 2:
        return P
    if np.allclose(P[0], P[-1]):
        far = int(np.argmax(np.linalg.norm(P - P[0], axis=1)))
        if far == 0:
            return P[:1]
        a = simplify(P[:far + 1], tol)
        b = simplify(P[far:], tol)
        return np.vstack([a[:-1], b])
    keep = np.zeros(len(P), dtype=bool)
    keep[0] = keep[-1] = True
    stack = [(0, len(P) - 1)]
    while stack:
        i, j = stack.pop()
        if j <= i + 1:
            continue
        a, b = P[i], P[j]
        d = b - a
        n = np.linalg.norm(d)
        seg = P[i + 1:j]
        if n < 1e-12:
            dist = np.linalg.norm(seg - a, axis=1)
        else:
            dist = np.abs(d[0] * (seg[:, 1] - a[1]) - d[1] * (seg[:, 0] - a[0])) / n
        k = int(np.argmax(dist))
        if dist[k] > tol:
            m = i + 1 + k
            keep[m] = True
            stack.append((i, m))
            stack.append((m, j))
    return P[keep]


# ----------------------------------------------------------------------------------------------
# Point location in an island's base triangles

INSIDE_EPS = 1e-7                       # barycentric tolerance for "inside the island": centres of real island triangles
                                        # are strictly inside; hull slivers along a border sit just outside


class TriLocator:
    """Finds which base triangle (in texel coordinates) holds a point, with a uniform grid."""

    def __init__(self, tris):
        self.t = np.asarray(tris, dtype=np.float64)               # (T, 3, 2)
        lo, hi = self.t.min(1), self.t.max(1)
        size = np.median(np.maximum(hi - lo, 1e-6).max(1))
        self.cell = max(float(size), 1.0)
        self.origin = lo.min(0)
        self.grid = {}
        for i, (a, b) in enumerate(zip(lo, hi)):
            c0 = ((a - self.origin) // self.cell).astype(int)
            c1 = ((b - self.origin) // self.cell).astype(int)
            for cx in range(c0[0], c1[0] + 1):
                for cy in range(c0[1], c1[1] + 1):
                    self.grid.setdefault((cx, cy), []).append(i)

    def locate(self, p):
        """(triangle index, barycentric weights); the nearest triangle if the point sits just outside all of them."""
        c = tuple(((np.asarray(p) - self.origin) // self.cell).astype(int))
        cand = self.grid.get(c, [])
        if not cand:
            cand = range(len(self.t))
        best, best_w, best_score = -1, None, -np.inf
        for i in cand:
            (ax, ay), (bx, by), (cx, cy) = self.t[i]
            d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(d) < 1e-12:
                continue
            l1 = ((by - cy) * (p[0] - cx) + (cx - bx) * (p[1] - cy)) / d
            l2 = ((cy - ay) * (p[0] - cx) + (ax - cx) * (p[1] - cy)) / d
            w = (l1, l2, 1.0 - l1 - l2)
            score = min(w)
            if score > best_score:
                best, best_w, best_score = i, w, score
                if score >= -1e-9:
                    break
        return best, best_w, best_score

    def inside_mask(self, x0, y0, w, h, erode=1):
        """Texel centres of a crop that lie inside the island (eroded by `erode` texels)."""
        m = np.zeros((h, w), dtype=bool)
        yy, xx = np.mgrid[0:h, 0:w]
        px, py = xx + x0 + 0.5, yy + y0 + 0.5
        for (ax, ay), (bx, by), (cx, cy) in self.t:
            d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(d) < 1e-12:
                continue
            l1 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / d
            l2 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / d
            m |= (l1 >= -1e-9) & (l2 >= -1e-9) & (1 - l1 - l2 >= -1e-9)
        for _ in range(erode):
            e = m.copy()
            e[1:, :] &= m[:-1, :]
            e[:-1, :] &= m[1:, :]
            e[:, 1:] &= m[:, :-1]
            e[:, :-1] &= m[:, 1:]
            m = e
        return m


# ----------------------------------------------------------------------------------------------
# Detail faces and islands

def uv_continuous(e, uvl):
    ls = e.link_loops
    if len(ls) != 2:
        return False
    a, b = ls
    ua = {a.vert: a[uvl].uv, a.link_loop_next.vert: a.link_loop_next[uvl].uv}
    ub = {b.vert: b[uvl].uv, b.link_loop_next.vert: b.link_loop_next[uvl].uv}
    return all(v in ub and (ua[v] - ub[v]).length < 1e-6 for v in ua)


def find_islands(faces, area):
    """Connected detail faces (through UV-continuous edges); one island never spans a UV seam."""
    index = {f: i for i, f in enumerate(faces)}
    seen = np.zeros(len(faces), dtype=bool)
    islands = []
    for i in np.nonzero(area)[0]:
        if seen[i]:
            continue
        seen[i] = True
        stack, members = [faces[i]], []
        while stack:
            f = stack.pop()
            members.append(f)
            for e in f.edges:
                if not uv_continuous(e, _UVL[0]):
                    continue
                for g in e.link_faces:
                    j = index.get(g)
                    if j is not None and area[j] and not seen[j]:
                        seen[j] = True
                        stack.append(g)
        islands.append(members)
    return islands


_UVL = [None]


# ----------------------------------------------------------------------------------------------
# Injection

def _cdt(points, edges):
    """Constrained Delaunay triangulation in texel space (output type 0: the full triangulation; triangles outside the
    island are dropped later). Returns verts, edges, faces, orig_verts, orig_edges."""
    out = mgeo.delaunay_2d_cdt([Vector(p) for p in points], edges, [], 0, 1e-4)
    return out[0], out[1], out[2], out[3], out[4]


def inject_island(bm, island, uvl, labelmap, dispmap, s, W, H, want_color, want_depth, flag_layer, stats):
    deform = bm.verts.layers.deform.active
    other_uv = [l for l in bm.loops.layers.uv.values() if l != uvl]
    faces = list(island)
    fset = set(faces)

    # island vertices, their UV in this island (texels), and corner data of the base triangles
    vlist, vindex, vuv = [], {}, []
    tris_px, tri_verts, tri_other_uv, tri_mat, tri_smooth = [], [], [], [], []
    orient = 0.0

    def key(l):
        return (l.vert, round(l[uvl].uv.x * W, 3), round(l[uvl].uv.y * H, 3))

    for f in faces:
        ls = list(f.loops)
        for l in ls:
            k = key(l)
            if k not in vindex:
                vindex[k] = len(vlist)
                vlist.append(l.vert)
                vuv.append((l[uvl].uv.x * W, l[uvl].uv.y * H))
        for k in range(1, len(ls) - 1):          # fan: faces may have gained corners from a neighbouring island's split
            tl = (ls[0], ls[k], ls[k + 1])
            corners = [(l[uvl].uv.x * W, l[uvl].uv.y * H) for l in tl]
            tris_px.append(corners)
            tri_verts.append([l.vert for l in tl])
            tri_other_uv.append([[l[o].uv.copy() for l in tl] for o in other_uv])
            tri_mat.append(f.material_index)
            tri_smooth.append(f.smooth)
            (ax, ay), (bx, by), (cx, cy) = corners
            orient += (bx - ax) * (cy - ay) - (cx - ax) * (by - ay)
    flip = orient < 0                                 # mirrored UVs: new faces must wind the other way round
    loc = TriLocator(tris_px)

    # input edges: island boundary always, base edges inside the island when kept (keeps the surface exact)
    edges_in, boundary_bmedges = [], []
    seen_e = set()
    for f in faces:
        for l in f.loops:
            e = l.edge
            a, b = vindex[key(l)], vindex[key(l.link_loop_next)]
            pair = (min(a, b), max(a, b))
            if pair in seen_e:
                continue
            seen_e.add(pair)
            inner = [g for g in e.link_faces if g in fset]
            on_border = len(inner) < 2 or not uv_continuous(e, uvl)
            if on_border or s.targeted_keep_base_edges:
                if on_border:
                    boundary_bmedges.append((len(edges_in), e, a, b))
                edges_in.append((a, b))
    n_base = len(vlist)
    points = list(vuv)

    # 1. color borders: traced contours, simplified, inside the island only
    if want_color:
        tp = np.array(tris_px)
        x0, y0 = int(math.floor(tp[..., 0].min())), int(math.floor(tp[..., 1].min()))
        x1, y1 = int(math.ceil(tp[..., 0].max())), int(math.ceil(tp[..., 1].max()))
        x0, y0 = max(x0, 0), max(y0, 0)
        x1, y1 = min(x1, labelmap.w), min(y1, labelmap.h)
        if x1 > x0 and y1 > y0:
            L = labelmap.a[y0:y1, x0:x1]
            inside = loc.inside_mask(x0, y0, x1 - x0, y1 - y0, erode=1)
            for chain in label_contours(L, inside):
                pts = simplify(np.array(chain, dtype=np.float64) + [x0, y0], s.targeted_tolerance_px)
                if len(pts) < 2:
                    continue
                ids = []
                for p in pts:
                    ids.append(len(points))
                    points.append((float(p[0]), float(p[1])))
                for a, b in zip(ids[:-1], ids[1:]):
                    edges_in.append((a, b))
                stats["contour_points"] += len(ids)

    # 2. height: add points where the triangulation still misses the fold depth, then triangulate again
    #    (points are appended, so the constraint edges keep their input indices)
    verts_out, edges_out, tris_out, orig_verts, orig_edges = _cdt(points, edges_in)
    if want_depth and dispmap is not None:
        from mathutils.kdtree import KDTree
        bary = np.array([[1 / 3, 1 / 3, 1 / 3], [0.6, 0.2, 0.2], [0.2, 0.6, 0.2], [0.2, 0.2, 0.6],
                         [0.5, 0.5, 0.0], [0.0, 0.5, 0.5], [0.5, 0.0, 0.5]])
        min_sp = s.targeted_min_spacing_px
        for _ in range(s.targeted_depth_iterations):
            V = np.array([[p.x, p.y] for p in verts_out])
            T = np.array([t for t in tris_out if len(t) == 3])
            if not len(T):
                break
            tri = V[T]
            area = 0.5 * np.abs((tri[:, 1, 0] - tri[:, 0, 0]) * (tri[:, 2, 1] - tri[:, 0, 1]) -
                                (tri[:, 2, 0] - tri[:, 0, 0]) * (tri[:, 1, 1] - tri[:, 0, 1]))
            cen = tri.mean(1)
            ok = np.array([loc.locate(c)[2] >= -INSIDE_EPS for c in cen])  # triangles inside the island
            dv = dispmap.bilinear(V / [W, H])
            S = np.einsum("pk,nkd->npd", bary, tri)
            err = np.abs(dispmap.bilinear(S / [W, H]) - dv[T] @ bary.T)
            worst = err.max(1)
            pick = np.nonzero(ok & (worst > s.depth_subdiv_threshold) & (area > min_sp * min_sp * 0.5))[0]
            if not len(pick):
                break
            kd = KDTree(len(V))
            for i, p in enumerate(V):
                kd.insert((p[0], p[1], 0.0), i)
            kd.balance()
            added = []
            akd = {}
            for t in pick[np.argsort(-worst[pick])]:
                # the worst sample that is strictly inside the island (edge midpoints can sit on its border, and a
                # point there would have to split the outside face too)
                q = None
                for si in np.argsort(-err[t]):
                    if err[t, si] <= s.depth_subdiv_threshold:
                        break
                    if loc.locate(S[t, si])[2] > 0.02:
                        q = S[t, si]
                        break
                if q is None or kd.find((q[0], q[1], 0.0))[2] < min_sp:
                    continue
                cell = (int(q[0] // min_sp), int(q[1] // min_sp))
                if any((cell[0] + dx, cell[1] + dy) in akd for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                    continue                       # keep new points min_sp apart from each other too
                akd[cell] = True
                added.append((float(q[0]), float(q[1])))
            if not added:
                break
            stats["depth_points"] += len(added)
            points = points + added
            verts_out, edges_out, tris_out, orig_verts, orig_edges = _cdt(points, edges_in)

    # vertices of the final triangulation: reuse base vertices, place the rest on the base triangles
    out_bm = [None] * len(verts_out)
    for k, ov in enumerate(orig_verts):
        for i in ov:
            if i < n_base:
                out_bm[k] = vlist[i]
                break

    # new points on island border edges: split those edges so the outside faces share the vertex
    on_edge = {}
    for j, oe in enumerate(orig_edges):
        for ie in oe:
            on_edge.setdefault(ie, []).append(j)
    border_verts = set(v for _, e, _, _ in boundary_bmedges for v in e.verts)
    for ie, e, ia, ib in boundary_bmedges:
        vs = set()
        for j in on_edge.get(ie, []):
            vs.update(edges_out[j])
        inner = [k for k in vs if out_bm[k] is None]
        if not inner:
            continue
        a, b = vlist[ia], vlist[ib]
        pa, pb = np.array(vuv[ia]), np.array(vuv[ib])
        d = pb - pa
        L2 = float(d @ d)
        if L2 < 1e-12:
            continue
        param = {k: float((np.array([verts_out[k].x, verts_out[k].y]) - pa) @ d) / L2 for k in inner}
        border_verts.update((a, b))
        cur_e, cur_v, prev = e, a, 0.0
        for k in sorted(inner, key=param.get):
            t = min(max(param[k], prev + 1e-6), 1 - 1e-6)
            fac = (t - prev) / (1.0 - prev)
            _, nv = bmesh.utils.edge_split(cur_e, cur_v, fac)
            out_bm[k] = nv
            border_verts.add(nv)
            stats["border_splits"] += 1
            nxt = [x for x in nv.link_edges if x.other_vert(nv) == b]
            if not nxt:
                break
            cur_e, cur_v, prev = nxt[0], nv, t

    # interpolated data for new vertices
    new_info = {}
    for k, p in enumerate(verts_out):
        if out_bm[k] is not None:
            continue
        ti, w, _ = loc.locate((p.x, p.y))
        tv = tri_verts[ti]
        co = tv[0].co * w[0] + tv[1].co * w[1] + tv[2].co * w[2]
        nv = bm.verts.new(co)
        if deform is not None:
            acc = {}
            for vv, ww in zip(tv, w):
                for g, gw in vv[deform].items():
                    acc[g] = acc.get(g, 0.0) + gw * max(ww, 0.0)
            tot = sum(max(x, 0.0) for x in w)
            for g, gw in acc.items():
                if gw > 0:
                    nv[deform][g] = gw / max(tot, 1e-9)
        out_bm[k] = nv
        new_info[k] = (ti, w)
        stats["new_vertices"] += 1

    # replace the island's faces
    for f in faces:
        if f.is_valid:
            bm.faces.remove(f)
    made = 0
    for t in tris_out:
        if len(t) != 3:
            continue
        c = np.array([[verts_out[k].x, verts_out[k].y] for k in t]).mean(0)
        ti, w, score = loc.locate(c)
        if score < -INSIDE_EPS:
            continue                                  # outside the island (convex hull of the triangulation)
        vs = [out_bm[k] for k in t]
        if len(set(vs)) < 3:
            continue
        order = [0, 2, 1] if flip else [0, 1, 2]
        try:
            nf = bm.faces.new([vs[i] for i in order])
        except ValueError:
            stats["skipped_faces"] += 1
            continue
        nf.material_index = tri_mat[ti]
        nf.smooth = tri_smooth[ti]
        for loop, i in zip(nf.loops, [t[i] for i in order]):
            loop[uvl].uv = (verts_out[i].x / W, verts_out[i].y / H)
            if other_uv:
                tj, wj, _ = loc.locate((verts_out[i].x, verts_out[i].y))
                for oi, layer in enumerate(other_uv):
                    u = tri_other_uv[tj][oi]
                    loop[layer].uv = u[0] * wj[0] + u[1] * wj[1] + u[2] * wj[2]
        made += 1
    stats["faces"] += made
    for v in out_bm:
        if v is not None and v.is_valid:
            v[flag_layer] = 2 if v in border_verts else 1


def targeted_inject(bm, uvl, labelmap, dispmap, s, faces_needing_refinement, tri_arrays, log):
    """Finds the detail islands on the (triangulated) base mesh and injects detail into each. Returns the vertex flag
    layer name ('_rft_island': 1 = inside an island, 2 = on an island border, 0 = untouched base mesh)."""
    _UVL[0] = uvl
    W, H = labelmap.w, labelmap.h
    faces, uv, vid = tri_arrays(bm, uvl)
    want_depth_any = dispmap is not None and s.targeted_depth_scope != "none"
    color, depth, mixed = faces_needing_refinement(uv, labelmap, dispmap if want_depth_any else None, s,
                                                   check_color=True, check_depth=want_depth_any)
    color_area = mixed | (color > 0)
    depth_area = depth > 0 if want_depth_any else np.zeros(len(faces), dtype=bool)
    if s.targeted_area == "selection":
        area = np.array([f.select for f in faces])
        if not area.any():
            raise RuntimeError("targeted_area 'selection': no faces are selected on the source mesh")
    elif s.targeted_depth_scope == "everywhere":
        area = color_area | depth_area
    else:
        area = color_area
    log("targeted prepass: %d of %d base faces crossed by color borders, %d need height detail; %d faces in islands" % (
        int(color_area.sum()), len(faces), int(depth_area.sum()), int(area.sum())))
    islands = find_islands(faces, area)
    flag = bm.verts.layers.int.new("_rft_island")
    stats = dict(contour_points=0, depth_points=0, border_splits=0, new_vertices=0, faces=0, skipped_faces=0)
    index = {f: i for i, f in enumerate(faces)}
    for isl in islands:
        idx = [index[f] for f in isl]
        want_color = bool(color_area[idx].any())
        want_depth = want_depth_any and (s.targeted_depth_scope == "everywhere" or want_color) \
            and (bool(depth_area[idx].any()) or s.targeted_area == "selection")
        inject_island(bm, isl, uvl, labelmap, dispmap, s, W, H, want_color, want_depth, flag, stats)
    # loose edges and vertices left where island faces were removed
    for e in [e for e in bm.edges if not e.link_faces]:
        bm.edges.remove(e)
    for v in [v for v in bm.verts if not v.link_edges]:
        bm.verts.remove(v)
    log("targeted: %d islands, %d contour points, %d height points, %d border splits, %d new vertices, %d island faces "
        "(%d skipped)" % (len(islands), stats["contour_points"], stats["depth_points"], stats["border_splits"],
                          stats["new_vertices"], stats["faces"], stats["skipped_faces"]))
    return "_rft_island"
