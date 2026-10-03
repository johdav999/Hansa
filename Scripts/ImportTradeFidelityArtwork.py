import unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir())
for name in ('Land','Sea','Linen','Rostock'):
 task=unreal.AssetImportTask()
 task.filename=str(root/'Content/Hansa/UI/TradeWorkspace'/f'{name}.png')
 task.destination_path='/Game/Hansa/UI/TradeWorkspace'
 task.destination_name=f'T_UI_TradeWorkspace_{name}_Default'
 task.automated=True;task.replace_existing=False;task.save=True
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 texture=unreal.load_asset(task.destination_path+'/'+task.destination_name)
 if not texture: raise RuntimeError('Import failed '+name)
 texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
 texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
 texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
 texture.set_editor_property('srgb',True)
 if name!='Rostock':
  texture.set_editor_property('address_x',unreal.TextureAddress.TA_WRAP)
  texture.set_editor_property('address_y',unreal.TextureAddress.TA_WRAP)
 unreal.EditorAssetLibrary.save_loaded_asset(texture)
 unreal.log('TRADE_FIDELITY_IMPORTED '+name)
