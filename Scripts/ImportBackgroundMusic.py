"""Import the two licensed Hansa background tracks as native, cooked SoundWaves."""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
DEST = "/Game/Hansa/Audio/Music"
TRACKS = (
    ("The Hansa's Harbor1.wav", "SW_HansasHarbor1"),
    ("The Hansa's Harbor 2.wav", "SW_HansasHarbor2"),
)
library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
for filename, asset_name in TRACKS:
    source = ROOT / "Audio" / "Music" / filename
    assert source.is_file(), source
    asset_path = f"{DEST}/{asset_name}"
    if not library.does_asset_exist(asset_path):
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DEST
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = False
        task.save = True
        tools.import_asset_tasks([task])
    wave = library.load_asset(asset_path)
    assert isinstance(wave, unreal.SoundWave), asset_path
    wave.set_editor_property("looping", False)
    assert library.save_loaded_asset(wave), asset_path
    unreal.log(f"Hansa music ready: {asset_path}, duration={wave.get_editor_property('duration')}")
