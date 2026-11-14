"""
AO Color Regions: Blender add-on (extension) around region_from_texture.py.

3D View > Sidebar > AO tab.
  Color Regions  settings (generated from region_from_texture.Settings, so new settings appear automatically) and Generate
  Region IDs     per-group region ID table, Apply IDs, select a group's faces, preview colors

The engine stays usable on its own: blender -b file.blend --python region_from_texture.py -- --option value ...
"""

import inspect
import math
import re
from dataclasses import fields

import bpy
from bpy.props import (BoolProperty, CollectionProperty, EnumProperty, FloatProperty, FloatVectorProperty,
                       IntProperty, PointerProperty, StringProperty)

from . import region_from_texture as core


# ----------------------------------------------------------------------------------------------
# Settings as Blender properties, generated from the engine's Settings dataclass

ENUMS = {
    "depth_source": [("none", "None", "No depth"),
                     ("luminance", "Luminance", "Darker painted folds are deeper"),
                     ("height", "Height Map", "Height image: brighter is higher"),
                     ("normal", "Normal Map", "Integrated tangent-space normal map")],
    "convention": [("0.2", "Step 0.2 (6 regions)", "Material multiplier 5"),
                   ("0.125", "Step 0.125 (9 regions)", "Material multiplier 8")],
    "method": [("subdivide", "Subdivide", "Refine the whole mesh where boundaries and folds need it"),
               ("targeted", "Targeted", "Keep the base mesh; inject detail islands only where color borders and folds need them")],
    "targeted_area": [("auto", "Automatic", "Base faces crossed by color borders, plus fold faces per Targeted depth scope"),
                      ("selection", "Selection", "Only the source mesh's selected faces")],
    "targeted_depth_scope": [("everywhere", "Everywhere", "Faces needing fold depth become islands too"),
                             ("color_areas", "Color areas", "Height detail only inside color islands"),
                             ("none", "None", "No height detail in islands")],
    "normal_green": [("directx", "DirectX (UE)", "Green points down"),
                     ("opengl", "OpenGL (Blender)", "Green points up")],
}
IMAGE_FIELDS = {"image_name", "depth_image"}       # drawn as image pickers
HIDDEN = {"object_name", "apply_ids"}               # the active object, and the Region IDs panel
MAIN_FIELDS = ("image_name", "uv_layer")            # drawn in the main panel, not a section


def parse_settings_source():
    """Sections ('# --- Name ---') and per-field comments of core.Settings, used as UI groups and tooltips."""
    sections, docs, last = [], {}, None
    for line in inspect.getsource(core.Settings).splitlines():
        m = re.match(r"\s*# --- (.+?) ---", line)
        if m:
            sections.append((m.group(1), []))
            last = None
            continue
        m = re.match(r"\s*(\w+):\s*\w+\s*=.*?(?:#\s*(.*))?$", line)
        if m:
            if not sections:
                sections.append(("General", []))
            sections[-1][1].append(m.group(1))
            docs[m.group(1)] = (m.group(2) or "").strip()
            last = m.group(1)
            continue
        m = re.match(r"\s*#\s*(.*)$", line)
        if m and last:
            docs[last] += " " + m.group(1).strip()
    return sections, docs


SECTIONS, DOCS = parse_settings_source()


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
    return type("AO_REGIONS_PG_settings", (bpy.types.PropertyGroup,), {"__annotations__": ann})


AO_REGIONS_PG_settings = make_settings_group()


def settings_for(context, obj, **overrides):
    props = context.scene.ao_regions
    values = {f.name: getattr(props, f.name) for f in fields(core.Settings) if f.name not in HIDDEN}
    values.update(object_name=obj.name, **overrides)
    return core.Settings(**values)


# ----------------------------------------------------------------------------------------------
# Per-object group table

class AO_REGIONS_PG_group(bpy.types.PropertyGroup):
    number: IntProperty(name="Group")
    faces: IntProperty(name="Faces")
    color: FloatVectorProperty(name="Color", subtype="COLOR", size=3, min=0.0, max=1.0,
                               description="The region's palette color (from RegionPreview)")
    region_id: IntProperty(name="Region ID", min=0, max=8, description="Region ID written for this group's faces")


def has_groups(obj):
    return obj is not None and obj.type == "MESH" and "RegionGroup" in obj.data.attributes


def refresh_groups(obj, prefix):
    """Rebuilds the table from the mesh, keeping IDs already chosen for groups that still exist."""
    import numpy as np
    me = obj.data
    kept = {g.number: g.region_id for g in obj.ao_region_groups}
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
    obj.ao_region_groups.clear()
    for g in sorted(int(x) for x in np.unique(grp)):
        item = obj.ao_region_groups.add()
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


def texture_mismatch(obj, image_name):
    """The images the object's materials use, when the chosen texture isn't one of them (else an empty list).
    Catches a Texture field left over from another object: the regions would follow the wrong texture."""
    if not image_name or obj is None or obj.type != "MESH":
        return []
    used = core.material_image_names(obj)
    return used if used and image_name not in used else []


def set_status(context, state, text):
    """Status line under Generate Regions (window-manager data: not saved with the file)."""
    wm = context.window_manager
    wm.ao_regions_status_state = state
    wm.ao_regions_status = text
    for window in wm.windows:
        for area in window.screen.areas:
            if area.type == "VIEW_3D":
                area.tag_redraw()


class AO_REGIONS_OT_generate(bpy.types.Operator):
    bl_idname = "ao_regions.generate"
    bl_label = "Generate Regions"
    bl_description = ("Make a copy of the active textured mesh with color regions cut into it, one vertex group per region "
                      "(Region1 = most faces), Shade and a RegionPreview color")
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        if obj is None or obj.type != "MESH":
            cls.poll_message_set("Select a textured mesh")
            return False
        if has_groups(obj) and not context.scene.ao_regions.in_place:
            cls.poll_message_set("This object already has regions; run on the original textured mesh")
            return False
        return True

    _timer = None
    _ticks = 0

    def invoke(self, context, event):
        # Blender doesn't redraw while an operator runs, so the work waits for a timer tick: the status line and the
        # wait cursor are drawn first, then the (blocking) run starts.
        set_status(context, "RUNNING", "In progress: %s\nBlender won't respond until it's done" % context.active_object.name)
        context.window.cursor_set("WAIT")
        self._ticks = 0
        self._timer = context.window_manager.event_timer_add(0.05, window=context.window)
        context.window_manager.modal_handler_add(self)
        return {"RUNNING_MODAL"}

    def modal(self, context, event):
        if event.type != "TIMER":
            return {"RUNNING_MODAL"}
        self._ticks += 1
        if self._ticks < 2:                     # one full redraw happens between the two ticks
            return {"RUNNING_MODAL"}
        context.window_manager.event_timer_remove(self._timer)
        try:
            return self.execute(context)
        finally:
            context.window.cursor_set("DEFAULT")

    def execute(self, context):
        src = context.active_object
        to_object_mode(context)
        s = settings_for(context, src)
        set_status(context, "RUNNING", "In progress: %s" % src.name)
        try:
            obj, report = core.run(s)
        except Exception as e:
            set_status(context, "FAILED", "Failed: %s" % e)
            self.report({"ERROR"}, str(e))
            return {"CANCELLED"}
        for o in context.selected_objects:
            o.select_set(False)
        obj.select_set(True)
        context.view_layer.objects.active = obj
        if has_groups(obj):
            refresh_groups(obj, s.group_prefix)
        done = "Done: %s\n%d groups, %d tris, %.0fs" % (
            obj.name, len(report["groups"]), report["triangles_after"], report["seconds"])
        used = texture_mismatch(src, s.image_name)
        if used:
            done += "\nTexture isn't on this object's material"
            self.report({"WARNING"}, "Texture %s isn't used by %s's material (it uses %s)" % (
                s.image_name, src.name, ", ".join(used[:2])))
        set_status(context, "DONE", done)
        self.report({"INFO"}, "%s: %d groups, %d -> %d triangles in %.1fs (details in the RegionReport text)" % (
            obj.name, len(report["groups"]), report["triangles_before"], report["triangles_after"], report["seconds"]))
        return {"FINISHED"}


# Presets: each sets only the settings that define its case. ("size", f) values are world distances given as a
# fraction of the active object's largest dimension, so they fit models of any scale (skipped with no active object).
# Never touched: texture, UV map, convention, analysis size, triangle target, method, groups/IDs.
PRESET_KEEP = ("image_name", "uv_layer")            # kept by the Defaults preset
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


class AO_REGIONS_OT_apply_preset(bpy.types.Operator):
    bl_idname = "ao_regions.apply_preset"
    bl_label = "Apply Preset"
    bl_description = ("Set the group of settings that defines a kind of surface. Texture, UV map, convention, analysis "
                      "size, triangle target and method are left alone (Defaults resets all but texture and UV map)")
    bl_options = {"REGISTER", "UNDO"}

    preset: EnumProperty(name="Preset", items=[(k, v[0], v[1]) for k, v in PRESETS.items()])

    def execute(self, context):
        props = context.scene.ao_regions
        label, _, values = PRESETS[self.preset]
        obj = context.active_object
        size = max(obj.dimensions) if obj is not None and obj.type == "MESH" else 0.0
        skipped = []
        if values is None:
            defaults = core.Settings()
            values = {f.name: getattr(defaults, f.name) for f in fields(core.Settings)
                      if f.name not in HIDDEN and f.name not in PRESET_KEEP}
        for name, value in values.items():
            if isinstance(value, tuple):
                if size <= 0:
                    skipped.append(name)
                    continue
                value = size * value[1]
            setattr(props, name, value)
        context.scene.ao_regions_preset = label           # saved with the file; settings may be edited afterwards
        note = " (%s skipped: no active mesh to scale them to)" % ", ".join(skipped) if skipped else ""
        self.report({"INFO"}, "Preset %s: %d settings set%s" % (label, len(values) - len(skipped), note))
        for window in context.window_manager.windows:
            for area in window.screen.areas:
                if area.type == "VIEW_3D":
                    area.tag_redraw()
        return {"FINISHED"}


class AO_REGIONS_OT_refresh(bpy.types.Operator):
    bl_idname = "ao_regions.refresh"
    bl_label = "Refresh Groups"
    bl_description = "Rebuild the group table from the mesh (keeps the IDs already chosen)"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return has_groups(context.active_object)

    def execute(self, context):
        to_object_mode(context)
        refresh_groups(context.active_object, context.scene.ao_regions.group_prefix)
        return {"FINISHED"}


class AO_REGIONS_OT_apply_ids(bpy.types.Operator):
    bl_idname = "ao_regions.apply_ids"
    bl_label = "Apply Region IDs"
    bl_description = ("Write RegionID (pre-compensated) and RegionIndex from the table. Faces assigned to another group by hand "
                      "go to that group. Giving several groups one ID joins them")
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return has_groups(obj) and len(obj.ao_region_groups) > 0

    def execute(self, context):
        obj = context.active_object
        to_object_mode(context)                 # vertex group edits reach the mesh data on leaving Edit Mode
        props = context.scene.ao_regions
        limit = len(core.region_value_table(props.convention)) - 1
        too_high = [g.name for g in obj.ao_region_groups if g.region_id > limit]
        if too_high:
            self.report({"ERROR"}, "%s: ID above %d, the %s convention's last region" % (", ".join(too_high), limit, props.convention))
            return {"CANCELLED"}
        mapping = ",".join("%d:%d" % (g.number, g.region_id) for g in obj.ao_region_groups)
        try:
            result = core.apply_region_ids(obj, settings_for(context, obj, apply_ids=mapping))
        except Exception as e:
            self.report({"ERROR"}, str(e))
            return {"CANCELLED"}
        refresh_groups(obj, props.group_prefix)
        self.report({"INFO"}, "RegionID written: %d faces, %d moved to match edited groups" % (result["faces"], result["moved"]))
        return {"FINISHED"}


class AO_REGIONS_OT_select_group(bpy.types.Operator):
    bl_idname = "ao_regions.select_group"
    bl_label = "Select Group Faces"
    bl_description = ("Select the faces generated for this group (by the RegionGroup attribute, so boundary faces of "
                      "neighboring groups aren't included). Shift: add to the selection")
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
        if layer is None:
            self.report({"ERROR"}, "No RegionGroup attribute")
            return {"CANCELLED"}
        if not self.extend:
            for f in bm.faces:
                f.select_set(False)
        count = 0
        for f in bm.faces:
            if f[layer] == self.number:
                f.select_set(True)
                count += 1
        bm.select_flush_mode()
        bmesh.update_edit_mesh(obj.data, loop_triangles=False, destructive=False)
        self.report({"INFO"}, "Selected %d faces of group %d" % (count, self.number))
        return {"FINISHED"}


class AO_REGIONS_OT_preview(bpy.types.Operator):
    bl_idname = "ao_regions.preview"
    bl_label = "Show Colors"
    bl_description = "Show a color attribute in Solid shading (RegionPreview, or RegionID to check the written IDs)"

    attribute: EnumProperty(items=[("RegionPreview", "Region Colors", "Palette color per region"),
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
                if shading.type not in ("SOLID",):
                    shading.type = "SOLID"
                shading.color_type = "VERTEX"
        return {"FINISHED"}


# ----------------------------------------------------------------------------------------------
# UI

class AO_REGIONS_UL_groups(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        row = layout.row(align=True)
        swatch = row.row()
        swatch.ui_units_x = 1.2
        swatch.prop(item, "color", text="")
        row.label(text=item.name)
        row.label(text="%d faces" % item.faces)
        row.prop(item, "region_id", text="ID")
        op = row.operator("ao_regions.select_group", text="", icon="RESTRICT_SELECT_OFF")
        op.number = item.number


def draw_field(layout, props, name, context):
    if name in IMAGE_FIELDS:
        layout.prop_search(props, name, bpy.data, "images", text=label_of(name))
    elif name == "uv_layer":
        obj = context.active_object
        if obj is not None and obj.type == "MESH":
            layout.prop_search(props, name, obj.data, "uv_layers", text="UV map")
        else:
            layout.prop(props, name)
    else:
        layout.prop(props, name)


class AO_REGIONS_PT_main(bpy.types.Panel):
    bl_idname = "AO_REGIONS_PT_main"
    bl_label = "Color Regions"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "AO"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        props = context.scene.ao_regions
        obj = context.active_object
        layout.label(text=obj.name if obj else "No active object", icon="MESH_DATA" if obj else "ERROR")
        col = layout.column()
        col.prop_search(props, "image_name", bpy.data, "images", text="Texture")
        if not props.image_name:
            col.label(text="Empty: the image feeding Base Color", icon="INFO")
        else:
            used = texture_mismatch(obj, props.image_name)
            if used:
                warn = col.column(align=True)
                warn.alert = True
                warn.label(text="Not used by this object's material", icon="ERROR")
                warn.label(text="It uses: %s" % ", ".join(used[:2]), icon="BLANK1")
        draw_field(col, props, "uv_layer", context)
        if not props.uv_layer:
            col.label(text="Empty: the active UV map", icon="INFO")
        wm = context.window_manager
        row = layout.row(align=True)
        row.operator_menu_enum("ao_regions.apply_preset", "preset", text="Preset", icon="PRESET")
        if context.scene.ao_regions_preset:
            row.label(text="Last: " + context.scene.ao_regions_preset.split(" (")[0])
        layout.operator("ao_regions.generate", icon="MOD_TRIANGULATE")
        if wm.ao_regions_status:
            icon = {"RUNNING": "TIME", "DONE": "CHECKMARK", "FAILED": "ERROR"}.get(wm.ao_regions_status_state, "INFO")
            col = layout.column(align=True)
            col.alert = wm.ao_regions_status_state == "FAILED"
            for i, line in enumerate(wm.ao_regions_status.split("\n")):
                col.label(text=line, icon=icon if i == 0 else "BLANK1")


def make_section_panel(index, title, names):
    names = [n for n in names if n not in HIDDEN and n not in MAIN_FIELDS]

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        props = context.scene.ao_regions
        col = layout.column()
        for n in names:
            draw_field(col, props, n, context)

    return type("AO_REGIONS_PT_section_%d" % index, (bpy.types.Panel,), {
        "bl_idname": "AO_REGIONS_PT_section_%d" % index,
        "bl_label": title,
        "bl_space_type": "VIEW_3D",
        "bl_region_type": "UI",
        "bl_category": "AO",
        "bl_parent_id": "AO_REGIONS_PT_main",
        "bl_options": {"DEFAULT_CLOSED"},
        "draw": draw,
    }) if names else None


class AO_REGIONS_PT_ids(bpy.types.Panel):
    bl_idname = "AO_REGIONS_PT_ids"
    bl_label = "Region IDs"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "AO"

    def draw(self, context):
        layout = self.layout
        obj = context.active_object
        if not has_groups(obj):
            layout.label(text="Generate regions, then pick the result", icon="INFO")
            return
        layout.template_list("AO_REGIONS_UL_groups", "", obj, "ao_region_groups", obj, "ao_region_groups_index", rows=4)
        row = layout.row(align=True)
        row.operator("ao_regions.refresh", icon="FILE_REFRESH")
        row.operator("ao_regions.apply_ids", icon="CHECKMARK")
        row = layout.row(align=True)
        row.operator("ao_regions.preview", text="Region Colors", icon="COLOR").attribute = "RegionPreview"
        sub = row.row(align=True)
        sub.enabled = "RegionID" in obj.data.color_attributes
        sub.operator("ao_regions.preview", text="Region IDs", icon="SEQUENCE_COLOR_04").attribute = "RegionID"
        props = context.scene.ao_regions
        limit = len(core.region_value_table(props.convention)) - 1
        col = layout.column(align=True)
        col.label(text="IDs 0-%d (%s convention)" % (limit, props.convention), icon="INFO")
        col.label(text="Groups given one ID become one region", icon="BLANK1")


SECTION_PANELS = [p for p in (make_section_panel(i, t, n) for i, (t, n) in enumerate(SECTIONS)) if p]

CLASSES = [AO_REGIONS_PG_settings, AO_REGIONS_PG_group,
           AO_REGIONS_OT_generate, AO_REGIONS_OT_apply_preset, AO_REGIONS_OT_refresh, AO_REGIONS_OT_apply_ids,
           AO_REGIONS_OT_select_group,
           AO_REGIONS_OT_preview, AO_REGIONS_UL_groups, AO_REGIONS_PT_main, *SECTION_PANELS, AO_REGIONS_PT_ids]


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    bpy.types.Scene.ao_regions = PointerProperty(type=AO_REGIONS_PG_settings)
    bpy.types.Object.ao_region_groups = CollectionProperty(type=AO_REGIONS_PG_group)
    bpy.types.Object.ao_region_groups_index = IntProperty()
    bpy.types.WindowManager.ao_regions_status = StringProperty(default="")
    bpy.types.WindowManager.ao_regions_status_state = StringProperty(default="")
    bpy.types.Scene.ao_regions_preset = StringProperty(default="")


def unregister():
    del bpy.types.Scene.ao_regions_preset
    del bpy.types.WindowManager.ao_regions_status_state
    del bpy.types.WindowManager.ao_regions_status
    del bpy.types.Object.ao_region_groups_index
    del bpy.types.Object.ao_region_groups
    del bpy.types.Scene.ao_regions
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
