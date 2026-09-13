"""Use deterministic full-detail static-mesh rendering for thin Brewery architecture."""
import unreal,json,sys
from pathlib import Path
JOB=Path(__file__).resolve().parents[1]
mesh=unreal.load_asset("/Game/Mesh/hansa-brewery-huexstrasse128/Production/SM_HansaBrewery_Production")
smes=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
settings=smes.get_nanite_settings(mesh)
settings.enabled=False
settings.set_editor_property("TargetMinimumResidencyInKB",0)
smes.set_nanite_settings(mesh,settings)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
unreal.log("BREWERY_CONVENTIONAL_RENDERING_SAVED")
sys.path.insert(0,str(JOB/"scripts"))
import capture_production
