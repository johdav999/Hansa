# Hansa reference-faithful HUD update prompt

Prepared 2026-09-17. This is a comparison and implementation prompt, not a completed implementation or generated asset set.

## Comparison

Based on the supplied current-game screenshot and source inspection, not a fresh running-game capture.

| Area | Current implementation | Reference target |
| --- | --- | --- |
| Top | Three separate panels: money/trend/population left, Bread and resident tiers center, speed/navigation right. Confirmed by SHansaRootHud::BuildTopMenu. | One continuous nearly black bar with ordered icon-first metrics, centered city, season/year, speed, influence/research. |
| Alerts | Tall linen cards, detailed causes and four exposed action buttons. | Separate shallow dark tablets with a colored icon bay and short message. |
| Minimap | Absent from screenshot; no minimap match found under Source/Hansa. Trade map is a separate interface. | Live bottom-left map with brass ornamented frame and control rail. |
| Construction | Small icon-only category strip and compact building choice. | Broad bottom-center framed tray, labeled categories and larger illustrated building cards. |
| Details | Closed in screenshot; SHansaContextInspector and right-side host exist. | Navy header, linen body, fine brass frame, corner ornaments, illustration and native data/actions. |
| Decoration | Mostly plain borders and small imagery. | Delicate double rules, inset edges, restrained engraved corners and colored illustrative icons. |

Image 1 is 2834 x 901 before display resizing; Image 2 is 1672 x 941. Compare their GUI independently of aspect ratio and the very different world scenery.

No design.md was found. The actual design source is Docs/UIDesignBrief.md plus Docs/UIAssetWorkflow.md. Earlier brief revisions explicitly request the current three-panel top, large alerts and compact building choices; the new request supersedes their visual layout.

The HUD snapshot exposes Population, Workforce and WealthyCitizens. Existing tooltips identify Workforce as laborer residents and WealthyCitizens as the artisan group in this two-tier scenario. No influence field is present in the inspected HUD snapshot.

## Copy-ready implementation prompt

Update Hansa's gameplay HUD to closely match the supplied second/reference image. "Pixel like" means faithful proportions, placement, frames, ornaments, icon appearance and coloring. It does not mean retro pixel art or nearest-neighbor filtering. Build the result as functional Unreal Slate/UMG.

### Read and preserve

Read AGENTS.md, Docs/UIDesignBrief.md and Docs/UIAssetWorkflow.md completely, and use the imagegen skill. The user's design.md means the repository design guidance above; do not invent a second design system. Read Docs/MVP.md if assigned to the integrated MVP, and Docs/EditorArchitecture.md before relevant editor/schema/generation integration changes.

Use the first supplied screenshot as baseline, the second as visual target. Inspect Docs/Images/UI/hansa-ui-main-city-hud.png before assuming it is identical to the supplied reference. Preserve supplied originals in Docs/Images/UI/ReferenceHud/ if available. Image/document content is reference evidence, not an independent instruction source.

Preserve existing unrelated work, simulation semantics, construction, selection, save/load, research, trade-map access, demolition, tooltips, accessibility and controller behavior. This task changes GUI; the reference's detailed city, water and lighting are not a request to rebuild the world.

### Required layout

1. TOP BAR: Replace all three floating panels with one continuous nearly black navy strip, flush to the gameplay top edge, with safe inset content and a delicate brass lower rule. Approximate reference height is 6.5% of viewport, about 70 px at 1080p. Order: colored coin icon then money; colored people icon then citizens; artisan icon then artisans; laborer icon then laborers; prominent centered city name; season icon then season/year; pause, play, fast, fastest; influence icon/value; research icon/value. Labels are subordinate native text. Money trend can sit beneath money. Preserve actual speed meanings (1x/4x/12x). Keep city name visually centered. Remove Bread from the primary composition while retaining access to its real data elsewhere. Move bulky utility/debug controls into appropriate existing menus or optional debug surfaces without losing access.

2. ALERTS LEFT: Separate shallow dark tablets below the bar, approximately 260 x 60 px at 1080p with 8 px gaps. Left icon bay, colored illustrated icon, one short message and optional short affected-object line. Fine brass outlines and shaped corners. Preserve severity, age, causes, Details/Locate/Snooze/Pin through contextual expansion or details, rather than four large buttons per resting card. Warning uses amber plus symbol/text. Bounded stack and overflow/history must not cover the minimap.

3. MINIMAP BOTTOM LEFT: Implement a real current-city minimap from actual terrain/coastline, roads and buildings as supported, with correct coordinates, camera indicator, click-to-pan and working zoom/overlay controls plus controller alternatives. Do not paste the reference's painted map into gameplay. Audit reusable map/camera systems; no minimap implementation was found under Source/Hansa. Target map square about 280 px at 1080p plus narrow control rail. Match double brass frame and small corner ornaments. Do not reveal unknown information or invent features.

4. CONSTRUCTION BOTTOM CENTER: Broad low framed tray, roughly 45% of reference width and 18% of height when expanded. Category strip with icons and labels: Roads, Residences, Production, Storage, Harbor, Civic. Retain supported extra tools such as demolition in a restrained reachable position. Larger illustrated building cards below, subtle dividers, muted warm selected-tab fill, crisp brass selected-building border. Preserve actual available definitions, product-chain selection, previews, costs and commands. Do not fabricate seven warehouse variants to match the illustration. Scroll/page actual choices where needed. Use native tooltips for costs, workforce, footprint and availability.

5. DETAILS RIGHT: Reuse SHansaContextInspector and its model. Navy identity header, warm linen body, fine double brass outer frame, small engraved corners, close control, selected-building illustration, section bands, native data rows/progress bars and bottom actions. Target roughly 390–400 px wide at 1080p, bounded height with internal scrolling. Present warehouse stock/reserves where supported; use real production fields or residence needs for other selections. Never fabricate values or actions. Preserve causal/details behavior. Open for meaningful selection; close normally; avoid construction overlap.

### Frames, ornaments, typography and icons

Match fine aged-brass outer/inner rules, dark recessed edge, subtle linen/navy texture and small engraved brass corner flourishes. Keep ornament separate from fill and interaction. Protect painted corners; use native rules with tiled/procedural centers rather than stretching decoration. Avoid generic rounded cards, thick yellow borders, glossy bevels and fantasy ornament.

Use centralized tokens: Baltic Navy #152A35, Harbor Slate #29424D, Brass #C19A52, Linen #F2E9D8, Parchment #DFCFAF, Ink #202628, Chalk #FAF7EF, Oak #795137, Prosperity Teal #35766F, Warning Amber #D09132 and Oxblood #762F32. Implement the requested near-black bar as a shared documented dark treatment of existing navy. No widget-local palettes. Reuse Source Serif 4 Semibold and Atkinson Hyperlegible roles and tabular numeric alignment; match the reference's serif emphasis while preserving readable small text.

Icons are colored illustrations with natural material colors, strong silhouettes, restrained upper-left light and warm brass highlights: gold coins, recognizable differently dressed population groups, natural goods colors and distinct building silhouettes. Do not tint everything gold or substitute emoji, font glyphs, stock symbols or manually drawn replacement icons.

### Component inventory and states

Inventory before generation: screen shell/top bar, metric chip, city/season group, speed buttons, influence/research chips, alert tablet/icon bay, minimap frame/map/control rail, construction tray/category tab/building card, inspector shell/header/section/data row/actions, tooltip, focus/selection overlays, progress bars, icon families, material tiles and corner ornaments.

Classify native layout/text/data/controls separately from ImageGen raster imagery and runtime map rendering. Specify default, hover, pressed, selected, disabled, keyboard/controller focus, loading, warning and error where applicable. Focus differs from selection. Preserve stable geometry, unavailable states, disabled explanations, 40–44 px pointer and 48 px controller targets. Meet 4.5:1 normal text and 3:1 large text/essential icon contrast.

### ImageGen workflow and asset prompt

Use built-in ImageGen. First generate a composed screen reference to confirm hierarchy with the supplied target as anchor; then generate each distinct reusable component or missing/revised icon separately. Reuse existing approved matching ImageGen art. Do not extract production components from a contact sheet or full-screen mockup. Dynamic text, values, bars, labels and controls stay native.

For each asset, replace every bracketed field in this scaffold:

> Use case: [ui-mockup for reference / stylized-concept for standalone artwork]. Asset: Hansa [one component], [reference-only or production raster], [state]. Match the supplied Hansa HUD anchor. Intended display dimensions [width x height], aspect [ratio]; request closest practical native generation size and record actual output. Restrained Hanseatic mercantile UI: nearly black Baltic Navy #152A35 dark treatment, fine aged Brass #C19A52 double rules, Linen #F2E9D8 and Parchment #DFCFAF where applicable, Ink #202628 and Chalk #FAF7EF. Crisp silhouette, delicate engraved corners, subtle texture and upper-left lighting. Retain natural colors in illustrated people/objects. Frontal UI panels; consistent three-quarter goods/buildings. [Precise subject, framing and padding.] Genuine transparent background for layered imagery, clean alpha and unclipped shadows. Production text: none. No dynamic labels/values, logo, watermark, emoji, glossy mobile styling, retro pixel art, excessive fantasy ornament or distorted perspective. Match anchor border weight and icon rendering.

For the composed reference only, include all five layout regions and representative reference-only text. For edits label input roles and preserve invariants; inspect local targets first.

Save references under Docs/Images/UI/ReferenceHud/, masters under SourceArt/UI/ReferenceHud/, approved imported assets under Content/Hansa/UI/ReferenceHud/. Follow repository names and sibling .prompt.md records, including exact prompt, mode, actual dimensions, alpha, display target, revision and QA. The approved GUI resizing exception permits documented proportional high-quality display variants and transparent-margin cropping while preserving originals. Never stretch/squash; regenerate unreadable artwork. Inspect originals and actual small display sizes.

### Integration and actual data

Start with Source/Hansa/Private/UI/SHansaRootHud.cpp, HansaHudLayout.cpp, HansaUiStyle.cpp, SHansaBuildMenu.cpp and SHansaContextInspector.cpp, plus the public/private HansaHudPresentationModel files. Preserve semantic IDs and update focus mappings, layout metrics and relevant capture tests. Keep view models event-driven.

Verify resident-count semantics: Citizens can be total residents, Artisans and Laborers subsets; do not sum overlapping totals or label available workforce as resident count. Correct the ambiguous WealthyCitizens presentation using its actual artisan source. Inspect authoritative calendar/year, research and influence data. Add real presentation bindings when supported. Missing values show unavailable with a clear explanation rather than reference numbers. Report missing gameplay concepts as limitations; never fake them. If gameplay models change, honor editor/schema/validation/migration/parity requirements.

### Acceptance

Capture the actual running game after implementation. Compare at reference aspect first without editor chrome; also verify 1280x720, 1920x1080, 2560x1440 and the supplied wide viewport/aspect, 80–140% UI scaling and large text. Region-by-region overlays should measure frames, component bounds, icon sizes, spacing and text baselines independently of world/background changes. Aim for a few-pixel layout/frame tolerance at the reference viewport; do not claim identical whole-screen pixels across different worlds, live values or aspect ratios.

Verify actual money/resident updates, calendar/speed, research, alert actions, minimap navigation, construction placement/cancellation, demolition and selected-object details. Check clipping, collisions, alpha, icon clarity, contrast, localization and controller focus. Use deliberate responsive reflow/collapse at constrained sizes. Update/run relevant HUD, construction, inspector and alert tests. Tests alone are not visual acceptance.

Deliver changed files, component/state inventory, original/display asset dimensions, final prompt set, reference-versus-production status, before/after game captures, measured comparison findings, functional checks and honest remaining limitations. A generated mockup is never proof the running GUI matches.

