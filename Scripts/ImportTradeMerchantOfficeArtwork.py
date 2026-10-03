import unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir())
source=root/'Content/Hansa/UI/TradeWorkspace/Icons/MerchantOffice--80.png'
task=unreal.AssetImportTask()
task.filename=str(source)
task.destination_path='/Game/Hansa/UI/TradeWorkspace'
task.destination_name='T_UI_TradeWorkspace_MerchantOffice_Default'
task.automated=True
task.replace_existing=False
task.save=True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture=unreal.load_asset(task.destination_path+'/'+task.destination_name)
if not texture:
    raise RuntimeError('Failed to import Merchant Office UI art')
texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('srgb',True)
texture.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP)
texture.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.log('TG11_MERCHANT_OFFICE_IMPORTED')
