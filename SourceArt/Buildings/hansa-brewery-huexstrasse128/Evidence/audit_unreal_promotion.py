"""Read-only saved Brewery production dependency and binding verification."""
import json
from pathlib import Path
import unreal

ROOT = "/Game/Mesh/hansa-brewery-huexstrasse128"
MESH_PATH = ROOT + "/Production/SM_HansaBrewery_Production"
JOB = Path(unreal.Paths.project_saved_dir()) / "GenerationJobs/hansa-brewery-huexstrasse128_20260912"
mesh = unreal.load_asset(MESH_PATH)
assert isinstance(mesh, unreal.StaticMesh), MESH_PATH
assert mesh.get_path_name() == MESH_PATH + ".SM_HansaBrewery_Production"

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous([ROOT], force_rescan=True)
options = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=True,
    include_hard_package_references=True,
    include_searchable_names=False,
    include_soft_management_references=False,
    include_hard_management_references=False)
queue = [MESH_PATH]
visited = {}
while queue:
    path = queue.pop()
    if path in visited:
        continue
    dependencies = [str(value) for value in registry.get_dependencies(path, options)]
    visited[path] = dependencies
    for dependency in dependencies:
        assert "/Staging/" not in dependency and "/Developer/" not in dependency, (path, dependency)
        if dependency.startswith("/Game/"):
            queue.append(dependency)

definition = unreal.load_asset("/Game/Hansa/Core/Buildings/DA_Building_Brewery")
assert definition.get_editor_property("presentation_mesh") == mesh
assert definition.get_editor_property("footprint_width_cells") == 4
assert definition.get_editor_property("footprint_height_cells") == 4
assert definition.get_editor_property("authored_revision") == 4
slots = mesh.get_editor_property("static_materials")
assert len(slots) == 16
nanite = mesh.get_editor_property("nanite_settings")
assert not nanite.enabled and nanite.explicit_tangents
assert nanite.fallback_percent_triangles == 1.0 and nanite.fallback_relative_error == 0.0
for family in ("Terracotta", "Terracotta_Dark", "Terracotta_Warm", "Terracotta_Smoked", "Terracotta_Pale"):
    material = unreal.load_asset(ROOT + "/R05/Materials/M_Brewery_" + family + "_R05")
    source_tint = unreal.load_object(None, material.get_path_name() + ":MaterialExpressionConstant3Vector_0")
    assert source_tint and source_tint.get_editor_property("constant").r < 0.7
for family in ("BrickWall", "BrickFace", "DarkBrick", "Oak"):
    material = unreal.load_asset(ROOT + "/R05/Materials/M_Brewery_" + family + "_R05")
    node = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_NORMAL)
    assert isinstance(node, unreal.MaterialExpressionNormalize), family
for slot in slots:
    assert slot.get_editor_property("material_interface") is not None
report = {
    "approval": "Johan explicitly approved brewery production promotion and game-data integration",
    "mesh": mesh.get_path_name(),
    "definition": definition.get_path_name(),
    "footprint": [4, 4],
    "material_slots": len(slots),
    "full_static_mesh_and_source_roof_tints": "passed",
    "nanite_enabled": False,
    "restrained_material_relief": "passed",
    "dependencies": visited,
    "production_dependency_audit": "passed"
}
with (JOB / "promotion-verification.json").open("w", encoding="utf-8") as output:
    json.dump(report, output, indent=2)
unreal.log("BREWERY_PRODUCTION_VERIFIED " + json.dumps(report))
