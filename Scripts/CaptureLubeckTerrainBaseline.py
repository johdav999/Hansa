"""Read-only snapshot of the user-named terrain starting point, without changing camera."""
import base64,json,struct
from InspectEnhancedMvpCities import JOB,call
expected='/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP'
assert call('editor_toolset.toolsets.scene.SceneTools','get_current_level')==expected
result=call('EditorToolset.EditorAppToolset','CaptureViewport',captureTransform=None,annotations=None,bShowUI=False)
data=base64.b64decode(result.pop('image')['data'],validate=True)
assert data[:8]==b'\x89PNG\r\n\x1a\n'
result['nativeDimensions']=struct.unpack('>II',data[16:24])
result['level']=expected
(JOB/'renders'/'lubeck-terrain-baseline.png').write_bytes(data)
(JOB/'evidence'/'lubeck-terrain-baseline.json').write_text(json.dumps(result,indent=2))
print('NATIVE_CAPTURE',len(data),result['nativeDimensions'])
