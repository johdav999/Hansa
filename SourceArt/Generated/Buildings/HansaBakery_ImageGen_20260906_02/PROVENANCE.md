# Provenance

Creation mode: four built-in ImageGen generate calls, followed by headless Blender 3.5.1 shader integration, native-size color baking, and Unreal MCP import. No API/CLI image generation, purchased scans, photograph uploads or photo texture pixels were used.

Four generated masters are synthetic surface-color artwork, not measured albedo or photogrammetric scans. Each is preserved at the returned native 1254 x 1254 dimensions with its exact sibling prompt record. The prompt requested 1024 square if supported; the actual native 1254 result was accepted without resizing. Source masters remain unmodified.

Brick/clay sources cover 0.5 x 0.5 m; plaster/oak cover 2 x 2 m. Nine color variants were baked from editable image/tint shaders at matching 1254-square resolution and one-to-one UV coverage. Existing independently authored procedural roughness and normal maps remain 1024 square. No luminance-to-height or luminance-to-roughness conversion was used. Brick/clay UV coverage was updated for the finer physical source extent, and 180 timber pieces received deterministic UV phase offsets while preserving grain direction and all-channel registration.

Native Unreal textures initially imported with NoMipmaps. Enabling FromTextureGroup mip generation visibly reduced distance noise while keeping the 1254-square source data. This is ordinary engine mip generation, not resizing the source master. Original engine masonry normal adaptation is retained; the revised oak uses 10 percent sampled normal blended with 90 percent flat normal.

Historical architectural references and inferred bakery details are unchanged from the original package; see reference_manifest.csv and its original provenance. No new historical accuracy claim is made. The asset remains a staging draft.
