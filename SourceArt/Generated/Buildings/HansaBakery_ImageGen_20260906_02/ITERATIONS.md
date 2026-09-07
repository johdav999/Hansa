# Material revision iterations

1. Inspected four native ImageGen outputs, then integrated them into nine image/tint color shaders. Rendered hero, shop, roof and four-repeat swatches. Plaster variation and irregular oak grain improve the previous procedural-only treatment; broad material swatches still reveal repeated oak knots.
2. Corrected repeated timber phase across 180 disconnected members, preserving vertical grain and common mapping across physical channels. Rerendered final views and verified the clean FBX shop / GLB hero results. Structural positions and topology remain unchanged.
3. Imported into an isolated Unreal preview. Read back all slot assignments and native generated color connections. Reduced retained oak normal weight to 10 percent for engine daylight; residual distance noise remained, so did not assume normals explained it.
4. Diagnosed NoMipmaps on all nine imported generated colors. Enabled FromTextureGroup mip generation without source padding/scaling; subsequent whole-building capture visibly reduced roof and timber noise. Captured final shop/roof views.
5. Packed-master check caught newly baked external image data; corrected packing, reopened and asserted 55 packed images, then rendered the full turntable and inspected rear frame 49. Clean GLB/FBX checks each found 51 maps and matching geometry bounds.

Temporary disk exhaustion was resolved by removing reproducible old clean-reimport scenes, automatic backups and byte-identical Saved export copies after verifying their persistent SourceArt originals. All original delivered artifacts remain preserved. This scoped material revision uses actual targeted corrections; it does not claim a new architectural modeling acceptance.
