"""
Restores the AO Color Regions panel to the v6 settings (Voodoo torso, 2026-11-09, `Mesh_0059_Regions_v6.001`).

Run inside Blender: Text Editor > Open > this file > Run Script. Every setting goes back to its default first, then the
v6 values are applied, so nothing from a later preset or experiment is left over. Works for the original add-on and for
the passes add-on (whichever are enabled).
"""

import sys

import bpy

V6 = {
    "image_name": "Voodoo_torso_d.dds",
    "uv_layer": "UVMap",
    "merge_distance": 0.1,
    "lightness_weight": 0.2,
    "analysis_size": 4096,
    "kmeans_restarts": 8,
    "convention": "0.125",
    "shade_group_distance": 0.035,
    "color_tolerance": 0.1,
    "min_color_share": 0.000001,
    "darker_tolerance_stops": 5.0,
    "method": "subdivide",
    "max_subdiv_levels": 6,
    "min_edge_length": 0.05,
    "min_split_fraction": 0.08,
    "dissolve_unneeded": False,
    "max_triangles": 300000,
    "reduce_keep_original_vertices": False,
    "shade_range": 0.94,
    "depth_source": "luminance",
    "depth_strength": 0.94,
    "depth_max": 1.0,
    "depth_midlevel": 0.1,
    "depth_blur_px": 1,
    "depth_subdiv_threshold": 0.015,
    "depth_subdiv_levels": 2,
}

# applies to whichever of the two add-ons is enabled: AO Color Regions (scene.ao_regions) and AO Color Region Passes
# (scene.aop_settings)
targets = [("ao_region_tools", "ao_regions", "ao_regions_preset"), ("ao_region_passes", "aop_settings", "aop_preset")]
done = []
for package, prop_name, preset_name in targets:
    core = next((m for n, m in sys.modules.items() if n.endswith(package + ".region_from_texture")), None)
    props = getattr(bpy.context.scene, prop_name, None)
    if core is None or props is None:
        continue
    defaults = core.Settings()
    for name in props.bl_rna.properties.keys():
        if name not in ("rna_type", "name") and hasattr(defaults, name):
            setattr(props, name, getattr(defaults, name))
    for name, value in V6.items():
        setattr(props, name, value)
    setattr(bpy.context.scene, preset_name, "v6")
    done.append(package)
if not done:
    raise RuntimeError("Neither AO Color Regions nor AO Color Region Passes is enabled")
print("Settings restored to v6 (%d values) in: %s" % (len(V6), ", ".join(done)))
