# Hansa repository instructions

These instructions apply to the entire repository.

## UI, GUI, and image work

### Required references

- Before planning, generating, reviewing, or implementing any GUI, UI, icon, illustration, texture, portrait, map treatment, or other game image, read `Docs/UIDesignBrief.md` and `Docs/UIAssetWorkflow.md` completely.
- Treat `Docs/UIDesignBrief.md` as the visual source of truth for palette, typography, materials, spacing, layout, accessibility, and interaction behavior.
- If a requested design direction conflicts with the brief, follow the user's explicit request and update the brief so future work uses the new direction.
- Do not invent a second palette, typography system, spacing scale, or component style inside an individual screen.

### Image generation

- Use the `imagegen` skill for every task that designs a new GUI screen, materially changes a GUI's visual appearance, creates or edits a GUI reference, or creates/edits raster game imagery. Pure implementation fixes that preserve an already approved visual design do not require a redundant generation pass.
- Use built-in ImageGen by default. Use an explicit API/CLI model only when the user requests or authorizes that workflow.
- Read the image-generation skill instructions before generating and follow its generate/edit, inspection, iteration, and project-save rules.
- Save every selected project asset in the repository. Never leave a project-referenced asset only in the generator's default output directory.
- For each new or revised final generated asset, preserve a sibling prompt record containing the final prompt, intended use, target dimensions/aspect ratio, generation mode, and revision notes.

### Component-first GUI workflow

- Before generating a screen, break it into a named component inventory. At minimum identify the screen shell, navigation, panels, tables/lists, controls, status/feedback, charts/overlays, icons, and decorative imagery.
- Define component states before artwork: default, hover, pressed, selected, disabled, keyboard/controller focus, loading, warning, and error where applicable.
- Generate a composed screen reference to confirm hierarchy, then generate a separate reference or asset for every distinct reusable visual component.
- Use one generation call per distinct component or variant. Do not generate a contact sheet and crop unrelated production components out of it.
- Reconstruct shipping screens from native UMG/Slate components and individual assets. Never ship a generated full-screen mockup as the interactive GUI.
- Treat generated layouts for tables, graphs, controls, and typography as references. Implement their functional form natively in UMG/Slate.

### Raster integrity and scaling

- Never stretch, squash, upscale, downscale, or otherwise resample a raster image to make it fit.
- Define the required pixel dimensions and aspect ratio before generation. Generate a new native-size variant for every required resolution, density, or aspect ratio.
- If the built-in generator cannot produce a mandatory exact size, generate again at a supported native size or ask whether to use the explicit API/CLI workflow. Never resize as a workaround.
- Render raster assets at 1:1 pixels in their intended presentation mode. Select dedicated `1x`, `1.5x`, `2x`, or aspect-ratio variants at runtime when needed.
- Never use a scale transform to conceal incorrect source dimensions.
- Preserve aspect ratio when placing reference images. Cropping transparent margins is allowed only when it does not alter or resample image content.
- For scalable geometry, use native Slate/UMG drawing, vector/SDF assets, materials, or separate resolution variants. Use tiled/procedural centers so nine-slice borders protect corners and edges without stretching painted detail.

### Production asset rules

- Never bake dynamic text, prices, quantities, player names, localization, key prompts, or changing status into a production raster asset. Render them with native text/widgets.
- Use transparent backgrounds for layered component art and preserve alpha. Reject halos, matte-colored fringes, dirty transparency, and clipped shadows.
- Keep decoration separate from functional surfaces so color, contrast, and layout can change without regenerating the ornament.
- Use image generation as a reference for small scalable icons, then recreate approved icons as vector/SDF or native shapes when practical.
- Every interactive component must have a readable focus state and must not communicate status by color alone.
- All text and essential icons must meet the contrast targets in `Docs/UIDesignBrief.md`.
- Leave localization expansion space. Do not size controls tightly around English reference text.
- Keep the design original. Historical research and genre conventions are allowed; copying another game's distinctive assets, exact layout, icon set, or branding is not.

### Consistency and visual QA

- Establish one approved style-anchor image for each asset family and use it as a reference for later generation in that family.
- Repeat exact palette tokens, material language, camera/viewpoint, lighting, line weight, and negative constraints in every related prompt.
- Inspect every generated output at original resolution before accepting it.
- Validate subject, silhouette, alpha, pixel dimensions, aspect ratio, palette, text accuracy, component state, safe margins, and consistency with the style anchor.
- Iterate with one targeted correction at a time and re-inspect the result.
- Keep rejected or superseded variants out of shipping asset folders. Preserve only deliberate comparison variants.
- Verify the assembled screen at every supported reference resolution and UI scale without resampling its raster components.

### Paths and naming

- Store visual mockups and non-shipping references under `Docs/Images/UI/<Feature>/`.
- Store generated source masters under `SourceArt/UI/<Feature>/`.
- Import production assets into `Content/Hansa/UI/<Feature>/`.
- Name generated masters `<feature>--<component>--<state>--<width>x<height>--v<number>.png`.
- Name Unreal textures `T_UI_<Feature>_<Component>_<State>`, widgets `WBP_<Feature>_<Component>`, and UI materials `M_UI_<Feature>_<Purpose>`.
- Store the prompt beside the generated master as the same basename with `.prompt.md`.

### Completion report

- Report the component inventory, final asset paths, native dimensions, generation mode, final prompt set, inspection results, and any remaining limitations.
- State clearly which outputs are visual references and which are production-ready imported assets.

## Editor and AI-assisted authoring work

- For any work assigned to the integrated Hansa MVP, read `Docs/MVP.md` and enforce its game/editor/automation parity contract, explicit exclusions, and acceptance gates.
- Before planning, implementing, or reviewing Hansa editor tools, game-definition schemas, AI-assisted data generation, generated 3D assets, rigging/animation generation, dialogue generation, or sound generation, read `Docs/EditorArchitecture.md` completely.
- For work assigned to the authoring-editor MVP, also read `Docs/EditorMVP.md` and enforce its explicit in-scope/out-of-scope boundary and acceptance gates.
- Treat the editor/game feature-parity contract in `Docs/EditorArchitecture.md` as part of the definition of done. A new or changed gameplay data model must update its editor schema, metadata, validation, migration, impact analysis, tests, and applicable generation/import workflow in the same implementation stream.
- Keep `HansaEditor` as an Editor-only module with one-way dependencies on runtime modules. Runtime modules must never depend on editor or provider-integration code.
- Keep OpenAI, TRELLIS, Tripo, ElevenLabs, credentials, downloads, and long-running generation work behind the external `HansaGenerationWorker`; do not embed provider SDKs or credentials in runtime modules.
- Generated data and media are drafts. Import them to staging, preserve provenance, validate them deterministically, show the actual diff or asset comparison, and require explicit approval before promotion to production content.
- Do not make provider task IDs, URLs, filenames, skeletons, voice IDs, or model versions part of gameplay identity. Use Hansa stable IDs and canonical asset contracts.
- Any new provider integration must implement the provider-neutral capability/job contract, cost controls, mocked contract tests, failure recovery, provenance, and Shipping-exclusion checks.
- Never make a live provider call from normal CI. Use mocks or recorded fixtures; live smoke tests require an explicitly budgeted workflow and dedicated low-privilege credentials.
- Prove that editor modules, generation workers, provider configuration/credentials, staging assets, and development-only manifests are absent from Shipping packages.
