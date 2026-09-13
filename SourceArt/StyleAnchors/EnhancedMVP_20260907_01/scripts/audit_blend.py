"""Emit deterministic source-scene measurements for an Enhanced MVP style anchor.

Run through Blender so the checked-in report comes from the editable master rather
than from an export manifest:
  blender --background <master.blend> --python audit_blend.py -- <asset-id> <report.json>
"""

import json
import math
import os
import sys

import bpy
from mathutils import Vector


def round_value(value):
    return round(float(value), 6)


def report():
    try:
        separator = sys.argv.index("--")
        asset_id, output_path = sys.argv[separator + 1 : separator + 3]
    except (ValueError, IndexError):
        raise RuntimeError("Expected -- <asset-id> <report.json>")

    scene = bpy.context.scene
    def is_review_geometry(obj):
        name = obj.name.lower()
        material_names = {
            slot.material.name.lower() for slot in obj.material_slots if slot.material
        }
        return (
            any("ground" in material and (
                "review" in material or "preview" in material
                or "qa_" in material or "neutral" in material
            ) for material in material_names)
            or name in {"ground", "review_ground", "preview_ground", "cyclorama"}
        )

    mesh_objects = [
        obj for obj in scene.objects
        if obj.type == "MESH" and not obj.hide_render and obj.data is not None
        and not is_review_geometry(obj)
    ]
    if not mesh_objects:
        raise RuntimeError("The render-visible source contains no mesh objects")

    corners = []
    triangle_count = 0
    vertex_count = 0
    uv_mesh_count = 0
    material_names = set()
    unapplied_scales = []
    for obj in mesh_objects:
        corners.extend(obj.matrix_world @ Vector(corner) for corner in obj.bound_box)
        evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        evaluated_mesh = evaluated.to_mesh()
        try:
            evaluated_mesh.calc_loop_triangles()
            triangle_count += len(evaluated_mesh.loop_triangles)
            vertex_count += len(evaluated_mesh.vertices)
            uv_mesh_count += 1 if evaluated_mesh.uv_layers else 0
        finally:
            evaluated.to_mesh_clear()
        material_names.update(slot.material.name for slot in obj.material_slots if slot.material)
        if any(not math.isclose(abs(axis), 1.0, abs_tol=1e-5) for axis in obj.scale):
            unapplied_scales.append({
                "object": obj.name,
                "scale": [round_value(axis) for axis in obj.scale],
            })

    minimum = Vector((min(p.x for p in corners), min(p.y for p in corners), min(p.z for p in corners)))
    maximum = Vector((max(p.x for p in corners), max(p.y for p in corners), max(p.z for p in corners)))
    size = maximum - minimum
    packed_images = sorted(
        image.name for image in bpy.data.images
        if image.source != "GENERATED" and image.packed_file is not None
    )
    external_images = sorted(
        image.name for image in bpy.data.images
        if image.source == "FILE" and image.packed_file is None and image.users > 0
    )

    result = {
        "schemaVersion": 1,
        "assetId": asset_id,
        "sourceBlend": bpy.data.filepath.replace("\\", "/"),
        "blenderVersion": bpy.app.version_string,
        "unitSystem": scene.unit_settings.system,
        "lengthUnit": scene.unit_settings.length_unit,
        "scaleLength": round_value(scene.unit_settings.scale_length),
        "renderVisibleMeshObjects": len(mesh_objects),
        "evaluatedVertices": vertex_count,
        "evaluatedTriangles": triangle_count,
        "boundsMeters": {
            "min": [round_value(axis) for axis in minimum],
            "max": [round_value(axis) for axis in maximum],
            "size": [round_value(axis) for axis in size],
        },
        "groundPivotOffsetMeters": round_value(minimum.z),
        "meshObjectsWithUv0": uv_mesh_count,
        "materialCount": len(material_names),
        "materials": sorted(material_names),
        "packedImageCount": len(packed_images),
        "packedImages": packed_images,
        "externalConsumedImages": external_images,
        "unappliedObjectScaleCount": len(unapplied_scales),
        "unappliedObjectScaleSamples": unapplied_scales[:25],
        "passed": (
            all(math.isfinite(axis) for point in corners for axis in point)
            and triangle_count > 0
            and uv_mesh_count == len(mesh_objects)
            and not external_images
        ),
    }
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with open(output_path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(result, handle, indent=2, ensure_ascii=False)
        handle.write("\n")
    print("HANSA_STYLE_ANCHOR_AUDIT=" + json.dumps(result, separators=(",", ":")))


report()
