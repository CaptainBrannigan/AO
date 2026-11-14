"""
AO Color Region Passes: the color-region tool split into separate passes (3D View > Sidebar > AO Passes).

  1. Analyze   texture -> region, shade and fold maps (saved images; no mesh change)
  2. Geometry  region map -> `<source>_Passes`, cut along every boundary (the only pass that adds vertices)
  3. Depth     fold map -> moves `<source>_Passes`'s vertices (re-runnable from the flat positions)
  4. Reduce    `<source>_Passes` -> `<source>_Passes_Reduced` (decimated to Max triangles)
  5. Region IDs

Separate from the original AO Color Regions add-on (own id, operators, properties and sidebar tab; own copy of the
engine), so both can be installed together.
"""

import inspect
import json
import math
import re
from dataclasses import fields

import bpy
from bpy.props import (BoolProperty, CollectionProperty, EnumProperty, FloatProperty, FloatVectorProperty,
                       IntProperty, PointerProperty, StringProperty)

from . import passes as P
from . import region_from_texture as core

CATEGORY = "AO Passes"

# ----------------------------------------------------------------------------------------------
# Settings as Blender properties (generated from the engine's Settings dataclass)

ENUMS = {
    "depth_source": [("none", "None", "No height: the fold map isn't made"),
                     ("luminance", "Luminance", "Darker than the region's light tone = inward (from the shade map)"),
                     ("height", "Height Map", "Height image: brighter is higher"),
                     ("normal", "Normal Map", "Integrated tangent-space normal map")],
    "convention": [("0.2", "Step 0.2 (6 regions)", "Material multiplier 5"),
                   ("0.125", "Step 0.125 (9 regions)", "Material multiplier 8")],
    "method": [("subdivide", "Subdivide", "Split faces where boundaries and folds need it, then cut boundaries into the edges"),
               ("targeted", "Targeted", "Keep the base mesh; rebuild only the faces boundaries or folds cross")],
    "targeted_area": [("auto", "Automatic", "Base faces crossed by color borders, plus fold faces per Targeted depth scope"),
                      ("selection", "Selection", "Only the source mesh's selected faces")],
    "targeted_depth_scope": [("everywhere", "Everywhere", "Faces needing fold depth become islands too"),
                             ("color_areas", "Color areas", "Height detail only inside color islands"),
                             ("none", "None", "No height detail in islands")],
    "ue_mesh_type": [("skeletal", "Skeletal mesh", "UE linearizes this mesh's vertex colors: values are written pre-compensated"),
                     ("static", "Static mesh", "UE passes this mesh's vertex colors through: plain values (region k = k x step)")],
    "normal_green": [("directx", "DirectX (UE)", "Green points down"),
                     ("opengl", "OpenGL (Blender)", "Green points up")],
}
IMAGE_FIELDS = {"image_name", "depth_image"}
HIDDEN = {"object_name", "apply_ids", "in_place", "output_suffix", "decimate_ratio"}
MAIN_FIELDS = ("image_name", "uv_layer")

# (pass, section title, fields); a pass with section None draws its fields directly in its panel
LAYOUT = [
    ("analyze", "Regions: what counts as one color",
     ["region_count", "merge_distance", "lightness_weight", "shade_group_distance", "region_merge_distance"]),
    ("analyze", "Small and separate colors",
     ["color_tolerance", "min_color_share", "darker_tolerance_stops", "lighter_tolerance_stops"]),
    ("analyze", "Shading", ["shade_range", "shade_reference_percentile"]),
    ("analyze", "Fold map: what becomes height",
     ["depth_source", "depth_image", "depth_regions", "depth_threshold", "depth_curve", "depth_blur_px",
      "depth_preserve_volume", "depth_volume_radius_px", "depth_edge_falloff_px", "depth_midlevel",
      "depth_highpass_px", "normal_green"]),
    ("analyze", "Map resolution and cleanup",
     ["analysis_size", "majority_radius", "majority_iterations", "convention", "palette", "index_remap",
      "cluster_samples", "kmeans_iterations", "kmeans_restarts", "seed", "auto_percentile", "auto_min_gain"]),
    ("geometry", "Method",
     ["method", "targeted_area", "targeted_depth_scope", "targeted_tolerance_px", "targeted_depth_iterations",
      "targeted_min_spacing_px", "targeted_keep_base_edges"]),
    ("geometry", "Vertices along color edges",
     ["max_subdiv_levels", "boundary_tolerance_px", "min_split_fraction", "min_edge_length", "sample_spacing_px",
      "max_samples_per_edge", "boundary_search_steps", "seam_inset"]),
    ("geometry", "Vertices along shade edges (inside regions)",
     ["shade_edge_stops", "shade_edge_blur_px", "shade_edge_cleanup_px"]),
    ("geometry", "Vertices for folds", ["depth_subdiv_levels", "depth_subdiv_threshold"]),
    ("geometry", "Cleanup", ["min_region_area", "dissolve_unneeded"]),
    ("geometry", "Output",
     ["hide_source", "write_shade", "ue_mesh_type", "write_preview", "write_vertex_groups", "group_prefix",
      "write_region_ids"]),
    ("depth", None, ["depth_strength", "depth_max", "depth_max_thickness", "depth_smooth_iterations"]),
    ("reduce", None, ["max_triangles", "reduce_protection_strength", "reduce_small_region_protection",
                      "reduce_shade_tolerance", "reduce_shade_edge_weight", "reduce_keep_original_vertices"]),
]
_placed = {n for _, _, names in LAYOUT for n in names} | HIDDEN | set(MAIN_FIELDS)
_unplaced = [f.name for f in fields(core.Settings) if f.name not in _placed]
if _unplaced:                                   # settings added to the engine later still show up somewhere
    LAYOUT.append(("analyze", "More", _unplaced))

PASS_INFO = {
    "analyze": ("1. Analyze", "VIEWZOOM", "Analyze",
                ["Texture -> 3 maps saved in this file.", "No mesh changes. Region map = colors,",
                 "fold map = height. Check them first."]),
    "geometry": ("2. Geometry", "MOD_TRIANGULATE", "Build Geometry",
                 ["Builds <source>_Passes, flat.", "The only pass that adds vertices:",
                  "on color and shade edges, folds."]),
    "depth": ("3. Depth", "MOD_DISPLACE", "Apply Depth",
              ["Moves vertices by the fold map.", "Adds no vertices. Starts from flat,",
               "so new values replace old ones."]),
    "reduce": ("4. Reduce", "MOD_DECIM", "Reduce",
               ["Copies to <source>_Passes_Reduced", "and decimates to Max triangles.",
                "Flat areas go first; borders kept."]),
}


def parse_docs():
    docs, last = {}, None
    for line in inspect.getsource(core.Settings).splitlines():
        m = re.match(r"\s*(\w+):\s*\w+\s*=.*?(?:#\s*(.*))?$", line)
        if m:
            docs[m.group(1)] = (m.group(2) or "").strip()
            last = m.group(1)
            continue
        m = re.match(r"\s*#\s*(.*)$", line)
        if m and last and not line.strip().startswith("# ---"):
            docs[last] += " " + m.group(1).strip()
    return docs


DOCS = parse_docs()


def label_of(name):
    return name.replace("_", " ").replace(" px", " (px)").capitalize()


def make_settings_group():
    defaults = core.Settings()
    ann = {}
    for f in fields(core.Settings):
        if f.name in HIDDEN:
            continue
        d, desc, label = getattr(defaults, f.name), DOCS.get(f.name, ""), label_of(f.name)
        if f.name in ENUMS:
            ann[f.name] = EnumProperty(name=label, description=desc, items=ENUMS[f.name], default=d)
        elif f.type is bool:
            ann[f.name] = BoolProperty(name=label, description=desc, default=d)
        elif f.type is int:
            ann[f.name] = IntProperty(name=label, description=desc, default=d, min=0)
        elif f.type is float:
            decimals = 2 if d == 0 or abs(d) >= 0.1 else min(6, 1 - int(math.floor(math.log10(abs(d)))))
            ann[f.name] = FloatProperty(name=label, description=desc, default=d, min=0.0, precision=max(decimals, 2), step=1)
        else:
            ann[f.name] = StringProperty(name=label, description=desc, default=d)
    return type("AOP_PG_settings", (bpy.types.PropertyGroup,), {"__annotations__": ann})


AOP_PG_settings = make_settings_group()


def settings_for(context, src, **overrides):
    props = context.scene.aop_settings
    values = {f.name: getattr(props, f.name) for f in fields(core.Settings) if f.name not in HIDDEN}
    values.update(object_name=src.name, **overrides)
    return core.Settings(**values)


# ----------------------------------------------------------------------------------------------
# Group table (Region IDs)

class AOP_PG_group(bpy.types.PropertyGroup):
    number: IntProperty(name="Group")
    faces: IntProperty(name="Faces")
    color: FloatVectorProperty(name="Color", subtype="COLOR", size=3, min=0.0, max=1.0)
    region_id: IntProperty(name="Region ID", min=0, max=8, description="Region ID written for this group's faces")


def has_groups(obj):
    return obj is not None and obj.type == "MESH" and "RegionGroup" in obj.data.attributes


def refresh_groups(obj, prefix):
    import numpy as np
    me = obj.data
    kept = {g.number: g.region_id for g in obj.aop_region_groups}
    grp = np.empty(len(me.polygons), dtype=np.int32)
    me.attributes["RegionGroup"].data.foreach_get("value", grp)
    counts = np.bincount(grp) if len(grp) else np.zeros(1, dtype=np.int64)
    colors = {}
    if "RegionPreview" in me.color_attributes:
        starts = np.empty(len(me.polygons), dtype=np.int64)
        me.polygons.foreach_get("loop_start", starts)
        col = np.empty(len(me.loops) * 4, dtype=np.float32)
        me.color_attributes["RegionPreview"].data.foreach_get("color", col)
        col = col.reshape(-1, 4)
        for g in np.unique(grp):
            colors[int(g)] = col[starts[np.argmax(grp == g)], :3]
    obj.aop_region_groups.clear()
    for g in sorted(int(x) for x in np.unique(grp)):
        item = obj.aop_region_groups.add()
        item.name = "%s%d" % (prefix, g)
        item.number = g
        item.faces = int(counts[g])
        item.color = tuple(float(c) for c in colors.get(g, (0.5, 0.5, 0.5)))
        item.region_id = kept.get(g, min(g - 1, 8))


# ----------------------------------------------------------------------------------------------
# Operators

def to_object_mode(context):
    if context.object and context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")


def redraw(context):
    for window in context.window_manager.windows:
        for area in window.screen.areas:
            if area.type in ("VIEW_3D", "IMAGE_EDITOR"):
                area.tag_redraw()


def set_status(context, state, text):
    wm = context.window_manager
    wm.aop_status_state = state
    wm.aop_status = text
    redraw(context)


def texture_mismatch(obj, image_name):
    if not image_name or obj is None or obj.type != "MESH":
        return []
    used = core.material_image_names(obj)
    return used if used and image_name not in used else []


class AOP_OT_run_pass(bpy.types.Operator):
    bl_idname = "aop.run_pass"
    bl_label = "Run Pass"
    bl_description = "Run this pass on the source mesh of the active object; later passes are marked out of date"
    bl_options = {"REGISTER", "UNDO"}

    step: EnumProperty(items=[("analyze", "Analyze", ""), ("geometry", "Geometry", ""), ("depth", "Depth", ""),
                              ("reduce", "Reduce", "")])
    _timer = None
    _ticks = 0

    @classmethod
    def poll(cls, context):
        if P.source_of(context.active_object) is None:
            cls.poll_message_set("Select the textured mesh (or one of its results)")
            return False
        return True

    def invoke(self, context, event):
        src = P.source_of(context.active_object)
        set_status(context, "RUNNING", "%s: %s\nBlender won't respond until it's done" % (PASS_INFO[self.step][0], src.name))
        context.window.cursor_set("WAIT")
        self._ticks = 0
        self._timer = context.window_manager.event_timer_add(0.05, window=context.window)
        context.window_manager.modal_handler_add(self)
        return {"RUNNING_MODAL"}

    def modal(self, context, event):
        if event.type != "TIMER":
            return {"RUNNING_MODAL"}
        self._ticks += 1
        if self._ticks < 2:
            return {"RUNNING_MODAL"}
        context.window_manager.event_timer_remove(self._timer)
        try:
            return self.execute(context)
        finally:
            context.window.cursor_set("DEFAULT")

    def execute(self, context):
        src = P.source_of(context.active_object)
        to_object_mode(context)
        s = settings_for(context, src)
        name = PASS_INFO[self.step][0]
        try:
            if self.step == "analyze":
                meta = P.analyze(src, s)
                text = "%s done: %d regions, %.0fs" % (name, meta["k"], meta["seconds"])
                obj = None
            else:
                fn = {"geometry": P.geometry, "depth": P.depth, "reduce": P.reduce}[self.step]
                obj, rec = fn(src, s)
                text = "%s done: %s, %.0fs" % (name, obj.name, rec["seconds"])
        except Exception as e:
            set_status(context, "FAILED", "%s failed: %s" % (name, e))
            self.report({"ERROR"}, str(e))
            return {"CANCELLED"}
        if obj is not None:
            for o in context.selected_objects:
                o.select_set(False)
            obj.hide_set(False)
            obj.select_set(True)
            context.view_layer.objects.active = obj
            if has_groups(obj):
                refresh_groups(obj, s.group_prefix)
        if texture_mismatch(src, s.image_name):
            text += "\nTexture isn't on the source's material"
        set_status(context, "DONE", text)
        return {"FINISHED"}


class AOP_OT_flatten(bpy.types.Operator):
    bl_idname = "aop.flatten"
    bl_label = "Flatten"
    bl_description = "Put <source>_Passes back to the flat positions Geometry saved (removes depth)"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        try:
            P.flatten(P.source_of(context.active_object))
        except Exception as e:
            self.report({"ERROR"}, str(e))
            return {"CANCELLED"}
        redraw(context)
        return {"FINISHED"}


class AOP_OT_show_map(bpy.types.Operator):
    bl_idname = "aop.show_map"
    bl_label = "Show Map"
    bl_description = "Open a saved map in an Image Editor (a new window if this screen has none)"

    kind: EnumProperty(items=[("region_map", "Regions", "Every texel's region, in the region's color; you can paint on it"),
                              ("shade_map", "Shade", "White = darkest tone of its region, black = its light tone"),
                              ("fold_map", "Folds", "Grey = no depth, darker = inward, lighter = outward")])

    def execute(self, context):
        src = P.source_of(context.active_object)
        meta = P._record(src, "aop_analysis") if src else None
        name = meta.get(self.kind) if meta else None
        img = bpy.data.images.get(name) if name else None
        if img is None:
            self.report({"WARNING"}, "No %s yet: run 1. Analyze%s" % (self.kind.replace("_", " "),
                        " with a depth source" if self.kind == "fold_map" else ""))
            return {"CANCELLED"}
        show_image(context, img)
        return {"FINISHED"}


def show_image(context, img):
    area = next((a for a in context.screen.areas if a.type == "IMAGE_EDITOR"), None)
    if area is None:
        bpy.ops.wm.window_new()
        win = context.window_manager.windows[-1]
        area = max(win.screen.areas, key=lambda a: a.width * a.height)
        area.type = "IMAGE_EDITOR"
    area.spaces.active.image = img


class AOP_OT_preview_shade_edges(bpy.types.Operator):
    bl_idname = "aop.preview_shade_edges"
    bl_label = "Preview Shade Edges"
    bl_description = ("Show the flat tones Geometry will cut with the current shade edge settings (each region's "
                      "color, darkened per shade step) as AOP_<source>_shade_edges. Fast: no mesh is built")

    @classmethod
    def poll(cls, context):
        return P.source_of(context.active_object) is not None

    def execute(self, context):
        src = P.source_of(context.active_object)
        try:
            img, n_steps, edge_px = P.preview_shade_edges(src, settings_for(context, src))
        except Exception as e:
            self.report({"ERROR"}, str(e))
            return {"CANCELLED"}
        show_image(context, img)
        if context.scene.aop_settings.shade_edge_stops <= 0:
            self.report({"INFO"}, "Shade edge stops is 0: only region borders are cut")
        else:
            self.report({"INFO"}, "%d shade steps, %d texel edges to follow (compare between settings)" % (n_steps, edge_px))
        return {"FINISHED"}


PRESET_KEEP = ("image_name", "uv_layer")
PRESETS = {
    "CLOTH": ("Cloth with creases", "Tones of one color fold into Shade; painted creases become depth", {
        "region_count": 0, "merge_distance": 0.06, "lightness_weight": 0.2, "shade_group_distance": 0.035,
        "color_tolerance": 0.1, "darker_tolerance_stops": 5.0, "lighter_tolerance_stops": 1.0,
        "majority_radius": 1, "majority_iterations": 2, "min_region_area": 0.0005,
        "shade_range": 0.94, "dissolve_unneeded": False,
        "max_subdiv_levels": 6, "depth_subdiv_levels": 2,
        "depth_source": "luminance", "depth_curve": 1.0, "depth_blur_px": 1,
        "depth_strength": ("size", 0.0077), "depth_max": ("size", 0.0082), "depth_subdiv_threshold": ("size", 0.000123),
    }),
    "CAMO": ("Camo (tones are regions)", "Three regions split by brightness; no tone folding, no depth", {
        "region_count": 3, "lightness_weight": 1.0, "shade_group_distance": 0.0, "region_merge_distance": 0.0,
        "color_tolerance": 0.0, "darker_tolerance_stops": 0.0, "lighter_tolerance_stops": 0.0,
        "majority_radius": 1, "majority_iterations": 2, "min_region_area": 0.00002,
        "shade_range": 0.6, "dissolve_unneeded": False, "depth_source": "none",
    }),
    "HARD": ("Hard surface (distinct colors)", "Automatic regions by hue, small distinct colors kept; no depth", {
        "region_count": 0, "merge_distance": 0.06, "lightness_weight": 0.35, "shade_group_distance": 0.0,
        "region_merge_distance": 0.0, "color_tolerance": 0.12, "darker_tolerance_stops": 4.0, "lighter_tolerance_stops": 1.0,
        "majority_radius": 1, "majority_iterations": 2, "min_region_area": 0.0005,
        "shade_range": 0.6, "dissolve_unneeded": False, "depth_source": "none",
    }),
    "DEFAULTS": ("Defaults", "Reset every setting except the texture and UV map", None),
}


class AOP_OT_apply_preset(bpy.types.Operator):
    bl_idname = "aop.apply_preset"
    bl_label = "Apply Preset"
    bl_description = ("Set the group of settings that defines a kind of surface. Texture, UV map, convention, analysis "
                      "size, triangle target and method are left alone (Defaults resets all but texture and UV map)")
    bl_options = {"REGISTER", "UNDO"}

    preset: EnumProperty(name="Preset", items=[(k, v[0], v[1]) for k, v in PRESETS.items()])

    def execute(self, context):
        props = context.scene.aop_settings
        label, _, values = PRESETS[self.preset]
        src = P.source_of(context.active_object)
        size = max(src.dimensions) if src is not None else 0.0
        if values is None:
            defaults = core.Settings()
            values = {f.name: getattr(defaults, f.name) for f in fields(core.Settings)
                      if f.name not in HIDDEN and f.name not in PRESET_KEEP}
        skipped = []
        for name, value in values.items():
            if isinstance(value, tuple):
                if size <= 0:
                    skipped.append(name)
                    continue
                value = size * value[1]
            setattr(props, name, value)
        context.scene.aop_preset = label
        self.report({"INFO"}, "Preset %s: %d settings set" % (label, len(values) - len(skipped)))
        redraw(context)
        return {"FINISHED"}


class AOP_OT_refresh(bpy.types.Operator):
    bl_idname = "aop.refresh_groups"
    bl_label = "Refresh Groups"
    bl_description = "Rebuild the group table from the mesh (keeps the IDs already chosen)"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return has_groups(context.active_object)

    def execute(self, context):
        to_object_mode(context)
        refresh_groups(context.active_object, context.scene.aop_settings.group_prefix)
        return {"FINISHED"}


class AOP_OT_apply_ids(bpy.types.Operator):
    bl_idname = "aop.apply_ids"
    bl_label = "Apply Region IDs"
    bl_description = ("Write RegionID and RegionIndex from the table, encoded for the UE mesh type (and re-encode Shade to "
                      "match). Faces assigned to another group by hand go to that group. Giving several groups one ID joins them")
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return has_groups(obj) and len(obj.aop_region_groups) > 0

    def execute(self, context):
        obj = context.active_object
        to_object_mode(context)
        props = context.scene.aop_settings
        limit = len(core.region_value_table(props.convention)) - 1
        too_high = [g.name for g in obj.aop_region_groups if g.region_id > limit]
        if too_high:
            self.report({"ERROR"}, "%s: ID above %d, the %s convention's last region" % (", ".join(too_high), limit, props.convention))
            return {"CANCELLED"}
        mapping = ",".join("%d:%d" % (g.number, g.region_id) for g in obj.aop_region_groups)
        try:
            result = core.apply_region_ids(obj, settings_for(context, obj, apply_ids=mapping))
        except Exception as e:
            self.report({"ERROR"}, str(e))
            return {"CANCELLED"}
        refresh_groups(obj, props.group_prefix)
        self.report({"INFO"}, "RegionID written: %d faces, %d moved to match edited groups" % (result["faces"], result["moved"]))
        return {"FINISHED"}


class AOP_OT_pack_export(bpy.types.Operator):
    bl_idname = "aop.pack_export"
    bl_label = "Pack for Export"
    bl_description = ("Copy the active result to <name>_Export with one color attribute, VertexColorPacked "
                      "(R = RegionID, G = 1, B = Shade, A = 1), the only vertex color set UE should import. "
                      "Export that copy as FBX (Selected Objects, Vertex Colors: Linear)")
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        if not has_groups(obj) or obj.get("aop_kind") == "export":
            return False
        if "RegionID" not in obj.data.color_attributes:
            cls.poll_message_set("Apply IDs first")
            return False
        return True

    def execute(self, context):
        obj = context.active_object
        to_object_mode(context)
        try:
            exp, rec = P.pack_for_export(obj, settings_for(context, P.source_of(obj) or obj))
        except Exception as e:
            self.report({"ERROR"}, str(e))
            return {"CANCELLED"}
        for o in context.selected_objects:
            o.select_set(False)
        obj.hide_set(True)
        exp.hide_set(False)
        exp.select_set(True)
        context.view_layer.objects.active = exp
        self.report({"INFO"}, "%s ready for a %s mesh: export it (Selected Objects, Vertex Colors: Linear)"
                    % (exp.name, rec["encoding"]))
        return {"FINISHED"}


class AOP_OT_select_group(bpy.types.Operator):
    bl_idname = "aop.select_group"
    bl_label = "Select Group Faces"
    bl_description = "Select this group's faces in Edit Mode (Shift: add to the selection)"
    bl_options = {"REGISTER", "UNDO"}

    number: IntProperty(name="Group")
    extend: BoolProperty(name="Extend", default=False)

    @classmethod
    def poll(cls, context):
        return has_groups(context.active_object)

    def invoke(self, context, event):
        self.extend = event.shift
        return self.execute(context)

    def execute(self, context):
        import bmesh
        obj = context.active_object
        if obj.mode != "EDIT":
            bpy.ops.object.mode_set(mode="EDIT")
        context.tool_settings.mesh_select_mode = (False, False, True)
        bm = bmesh.from_edit_mesh(obj.data)
        layer = bm.faces.layers.int.get("RegionGroup")
        if not self.extend:
            for f in bm.faces:
                f.select_set(False)
        for f in bm.faces:
            if f[layer] == self.number:
                f.select_set(True)
        bm.select_flush_mode()
        bmesh.update_edit_mesh(obj.data, loop_triangles=False, destructive=False)
        return {"FINISHED"}


class AOP_OT_preview(bpy.types.Operator):
    bl_idname = "aop.preview"
    bl_label = "Show Colors"
    bl_description = "Show a color attribute in Solid shading (RegionPreview, Shade, or RegionID)"

    attribute: EnumProperty(items=[("RegionPreview", "Region Colors", "Palette color per region"),
                                   ("Shade", "Shade", "The Shade values written per face corner"),
                                   ("RegionID", "Region IDs", "The written RegionID values")])

    @classmethod
    def poll(cls, context):
        return has_groups(context.active_object)

    def execute(self, context):
        me = context.active_object.data
        if self.attribute not in me.color_attributes:
            self.report({"WARNING"}, "%s hasn't been written yet" % self.attribute)
            return {"CANCELLED"}
        me.color_attributes.active_color = me.color_attributes[self.attribute]
        for area in context.screen.areas:
            if area.type == "VIEW_3D":
                shading = area.spaces.active.shading
                shading.type = "SOLID"
                shading.color_type = "VERTEX"
        return {"FINISHED"}


# ----------------------------------------------------------------------------------------------
# UI

class AOP_UL_groups(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        row = layout.row(align=True)
        swatch = row.row()
        swatch.ui_units_x = 1.2
        swatch.prop(item, "color", text="")
        row.label(text=item.name)
        row.label(text="%d faces" % item.faces)
        row.prop(item, "region_id", text="ID")
        row.operator("aop.select_group", text="", icon="RESTRICT_SELECT_OFF").number = item.number


def draw_field(layout, props, name, context):
    if name in IMAGE_FIELDS:
        layout.prop_search(props, name, bpy.data, "images", text=label_of(name))
    elif name == "uv_layer":
        src = P.source_of(context.active_object)
        if src is not None:
            layout.prop_search(props, name, src.data, "uv_layers", text="UV map")
        else:
            layout.prop(props, name)
    else:
        layout.prop(props, name)


def _panel(idname, label, draw, parent=None, closed=False, order=0):
    attrs = {"bl_idname": idname, "bl_label": label, "bl_space_type": "VIEW_3D", "bl_region_type": "UI",
             "bl_category": CATEGORY, "bl_order": order, "draw": draw}
    if parent:
        attrs["bl_parent_id"] = parent
    if closed:
        attrs["bl_options"] = {"DEFAULT_CLOSED"}
    return type(idname, (bpy.types.Panel,), attrs)


def draw_main(self, context):
    layout = self.layout
    layout.use_property_split = True
    layout.use_property_decorate = False
    props = context.scene.aop_settings
    obj = context.active_object
    src = P.source_of(obj)
    if src is None:
        layout.label(text="Select a textured mesh", icon="ERROR")
    else:
        layout.label(text="Source: %s" % src.name, icon="MESH_DATA")
        if obj is not src:
            layout.label(text="Active: %s" % obj.name, icon="BLANK1")
        full = P.result_of(src, "full")
        if full is not None:
            layout.label(text="Geometry result: %s" % full.name, icon="BLANK1")
        if P.is_snapshot(obj):
            warn = layout.column(align=True)
            warn.alert = True
            warn.label(text="This is a copy. Passes work on", icon="ERROR")
            warn.label(text=P.result_of(src, obj.get("aop_kind")).name, icon="BLANK1")
    col = layout.column()
    col.prop_search(props, "image_name", bpy.data, "images", text="Texture")
    if not props.image_name:
        col.label(text="Empty: the image feeding Base Color", icon="INFO")
    else:
        used = texture_mismatch(src, props.image_name)
        if used:
            warn = col.column(align=True)
            warn.alert = True
            warn.label(text="Not used by the source's material", icon="ERROR")
            warn.label(text="It uses: %s" % ", ".join(used[:2]), icon="BLANK1")
    draw_field(col, props, "uv_layer", context)
    row = layout.row(align=True)
    row.operator_menu_enum("aop.apply_preset", "preset", text="Preset", icon="PRESET")
    if context.scene.aop_preset:
        row.label(text="Last: " + context.scene.aop_preset.split(" (")[0])
    col = layout.column(align=True)
    col.label(text="Run the passes in order.", icon="INFO")
    col.label(text="Each saves its result; re-running", icon="BLANK1")
    col.label(text="one marks later ones out of date.", icon="BLANK1")
    wm = context.window_manager
    if wm.aop_status:
        icon = {"RUNNING": "TIME", "DONE": "CHECKMARK", "FAILED": "ERROR"}.get(wm.aop_status_state, "INFO")
        col = layout.column(align=True)
        col.alert = wm.aop_status_state == "FAILED"
        for i, line in enumerate(wm.aop_status.split("\n")):
            col.label(text=line, icon=icon if i == 0 else "BLANK1")


def make_pass_draw(step):
    title, icon, button, lines = PASS_INFO[step]
    direct = [names for p, sec, names in LAYOUT if p == step and sec is None]

    def draw(self, context):
        layout = self.layout
        props = context.scene.aop_settings
        src = P.source_of(context.active_object)
        if src is not None:
            state, text = P.status(src, settings_for(context, src)).get(step, ("none", ""))
            box = layout.box()
            box.alert = state == "stale"
            head = {"done": "Done", "stale": "Out of date", "none": "Not run"}[state]
            body = text if state == "none" and text != "Not run" else (head + (": " + text if text and text != "Not run" else ""))
            box.label(text=body.split(" | ")[0],
                      icon={"done": "CHECKMARK", "stale": "ERROR", "none": "RADIOBUT_OFF"}[state])
            for extra in body.split(" | ")[1:]:
                box.label(text=extra, icon="BLANK1")
        layout.operator("aop.run_pass", text=button, icon=icon).step = step
        if step == "analyze":
            row = layout.row(align=True)
            for kind, label in (("region_map", "Regions"), ("shade_map", "Shade"), ("fold_map", "Folds")):
                row.operator("aop.show_map", text=label, icon="IMAGE_DATA").kind = kind
        if step == "depth":
            layout.operator("aop.flatten", icon="LOOP_BACK")
            full = P.result_of(src) if src else None
            geo = P._record(full, "aop_geometry") if full else None
            if geo and props.depth_strength > 0 and abs(geo["depth_strength_used"] - props.depth_strength) > 0.25 * props.depth_strength:
                c = layout.column(align=True)
                c.label(text="Geometry used strength %.3g." % geo["depth_strength_used"], icon="INFO")
                c.label(text="Re-run 2 if creases look coarse.", icon="BLANK1")
        col = layout.column(align=True)
        for line in lines:
            col.label(text=line)
        if direct:
            col = layout.column()
            col.use_property_split = True
            col.use_property_decorate = False
            for names in direct:
                for n in names:
                    draw_field(col, props, n, context)
    return draw


def make_section_draw(names):
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        props = context.scene.aop_settings
        col = layout.column()
        for n in names:
            draw_field(col, props, n, context)
        if "shade_edge_stops" in names:
            layout.operator("aop.preview_shade_edges", icon="IMAGE_DATA")
    return draw


def draw_ids(self, context):
    layout = self.layout
    obj = context.active_object
    if obj is not None and obj.get("aop_kind") == "export":
        col = layout.column(align=True)
        col.label(text="Export copy: VertexColorPacked only.", icon="EXPORT")
        col.label(text="File > Export > FBX: Selected", icon="BLANK1")
        col.label(text="Objects, Vertex Colors: Linear.", icon="BLANK1")
        return
    if not has_groups(obj):
        layout.label(text="Build geometry, then select the result", icon="INFO")
        return
    props = context.scene.aop_settings
    col = layout.column(align=True)
    col.use_property_split = True
    col.use_property_decorate = False
    col.prop(props, "ue_mesh_type", text="Import in UE as")
    current = core.mesh_encoding(obj.data)
    if current != props.ue_mesh_type:
        warn = col.column(align=True)
        warn.alert = True
        warn.label(text="Written for a %s mesh." % current, icon="ERROR")
        warn.label(text="Apply IDs to re-encode.", icon="BLANK1")
    layout.template_list("AOP_UL_groups", "", obj, "aop_region_groups", obj, "aop_region_groups_index", rows=4)
    row = layout.row(align=True)
    row.operator("aop.refresh_groups", icon="FILE_REFRESH")
    row.operator("aop.apply_ids", icon="CHECKMARK")
    row = layout.row(align=True)
    row.operator("aop.preview", text="Colors", icon="COLOR").attribute = "RegionPreview"
    row.operator("aop.preview", text="Shade", icon="MOD_TINT").attribute = "Shade"
    sub = row.row(align=True)
    sub.enabled = "RegionID" in obj.data.color_attributes
    sub.operator("aop.preview", text="IDs", icon="SEQUENCE_COLOR_04").attribute = "RegionID"
    layout.operator("aop.pack_export", icon="EXPORT")


PANELS = [_panel("AOP_PT_main", "Color Region Passes", draw_main, order=0)]
for i, step in enumerate(("analyze", "geometry", "depth", "reduce")):
    pid = "AOP_PT_pass_%s" % step
    PANELS.append(_panel(pid, PASS_INFO[step][0], make_pass_draw(step), order=i + 1))
    for j, (p, sec, names) in enumerate(LAYOUT):
        if p == step and sec is not None:
            PANELS.append(_panel("AOP_PT_%s_%d" % (step, j), sec, make_section_draw(names), parent=pid, closed=True, order=j))
PANELS.append(_panel("AOP_PT_ids", "5. Region IDs", draw_ids, order=10))

CLASSES = [AOP_PG_settings, AOP_PG_group, AOP_OT_run_pass, AOP_OT_flatten, AOP_OT_show_map, AOP_OT_preview_shade_edges,
           AOP_OT_apply_preset,
           AOP_OT_refresh, AOP_OT_apply_ids, AOP_OT_pack_export, AOP_OT_select_group, AOP_OT_preview, AOP_UL_groups, *PANELS]


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    bpy.types.Scene.aop_settings = PointerProperty(type=AOP_PG_settings)
    bpy.types.Scene.aop_preset = StringProperty(default="")
    bpy.types.Object.aop_region_groups = CollectionProperty(type=AOP_PG_group)
    bpy.types.Object.aop_region_groups_index = IntProperty()
    for prop in P.RESULT_POINTERS.values():
        setattr(bpy.types.Object, prop, PointerProperty(type=bpy.types.Object))
    bpy.types.WindowManager.aop_status = StringProperty(default="")
    bpy.types.WindowManager.aop_status_state = StringProperty(default="")


def unregister():
    del bpy.types.WindowManager.aop_status_state
    del bpy.types.WindowManager.aop_status
    del bpy.types.Object.aop_region_groups_index
    for prop in P.RESULT_POINTERS.values():
        delattr(bpy.types.Object, prop)
    del bpy.types.Object.aop_region_groups
    del bpy.types.Scene.aop_preset
    del bpy.types.Scene.aop_settings
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
