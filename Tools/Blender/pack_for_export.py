"""
Packs a color-region mesh's attributes into the one vertex color set UE imports, on an export copy.

Run inside Blender with the finished mesh (e.g. Mesh_0059_Passes_Reduced) selected and active:
Scripting workspace > Text Editor > Open > this file > Run Script. (Not the Python Console: it runs pasted
blocks line by line and stops at the first indented block.)

Makes <mesh>_Export with a single Face Corner color attribute, VertexColorPacked:
    R = RegionID (pre-compensated region ID), G = 1 (AO, unused for now), B = Shade, A = 1
and leaves it selected and active. The source mesh is not changed. Export it with
File > Export > FBX: Limit to Selected Objects (plus its armature), Geometry > Vertex Colors = Linear.
"""

import bpy
import numpy as np

src = bpy.context.active_object
if src is None or src.type != "MESH":
    raise RuntimeError("Select the mesh to pack (make it the active object)")
if src.name.endswith("_Export"):
    raise RuntimeError("%s is already an export copy; select the source mesh" % src.name)
for name in ("RegionID", "Shade"):
    a = src.data.color_attributes.get(name)
    if a is None:
        raise RuntimeError("%s has no %s attribute%s" % (src.name, name,
                           " (run Apply IDs in the Region IDs panel first)" if name == "RegionID" else ""))
    if a.domain != "CORNER":
        raise RuntimeError("%s on %s must be Face Corner, not %s" % (name, src.name, a.domain))

# a previous copy (including one left unlinked by an interrupted run) is replaced
old = bpy.data.objects.get(src.name + "_Export")
if old is not None:
    old_me = old.data
    bpy.data.objects.remove(old)
    if old_me is not None and old_me.users == 0:
        bpy.data.meshes.remove(old_me)

obj = src.copy()
obj.data = src.data.copy()
obj.name = src.name + "_Export"
obj.data.name = obj.name
for col in src.users_collection:
    col.objects.link(obj)

me = obj.data
n = len(me.loops)


def corner(name):
    c = np.empty(n * 4, np.float32)
    me.color_attributes[name].data.foreach_get("color", c)
    return c.reshape(-1, 4)


packed = np.ones((n, 4), np.float32)        # G (AO) and A (spare) = 1
packed[:, 0] = corner("RegionID")[:, 0]     # R = region ID, as the add-on wrote it (pre-compensated)
packed[:, 2] = corner("Shade")[:, 0]        # B = shade, as the add-on wrote it (pre-compensated)

# the export copy keeps nothing but the packed set. Removed by name, one lookup at a time: removing an attribute
# invalidates Python references to the others, so removing from a list of references can skip some
for name in [a.name for a in me.color_attributes]:
    a = me.color_attributes.get(name)
    if a is not None:
        me.color_attributes.remove(a)
p = me.color_attributes.new("VertexColorPacked", "FLOAT_COLOR", "CORNER")
p.data.foreach_set("color", packed.ravel())
left = [a.name for a in me.color_attributes]
if left != ["VertexColorPacked"]:
    raise RuntimeError("%s still has color attributes %s; remove all but VertexColorPacked before export" % (obj.name, left))
me.color_attributes.active_color = me.color_attributes["VertexColorPacked"]
me.color_attributes.render_color_index = 0

for o in bpy.context.selected_objects:
    o.select_set(False)
src.hide_set(True)
obj.hide_set(False)
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
ids = np.unique(np.round(packed[:, 0], 4))
print("Packed %s: %d corners, RegionID values %s" % (obj.name, n, ids.tolist()))
