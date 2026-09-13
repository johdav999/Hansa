# GUI ImageGen migration — 2026-09-10

The shared GUI pictograms now render actual ImageGen PNG artwork. Former procedural pictograms in SHansaGlyph, the trade-map ship, market goods, search controls, inspector artwork, and symbolic icon buttons use generated assets. Legacy decorative font prefixes were removed; alerts have generated severity icons. Mathematical notation, live text, charts, route geometry, widget surfaces and focus outlines remain native Slate.

## Component inventory and scope

- Screen shell and navigation: title, settings, credits, session menu, new-game/scenario, save/load and confirmations; existing native composition preserved.
- Three top panels: treasury/monthly trend/population; top products and citizen tiers; speed controls and navigation.
- Construction: category icons, goods and building cards, placement status.
- Inspectors: production recipe ports, batch/cost/workforce indicators, citizen needs, worker and citizen portraits.
- City/market: all ten goods, search/clear and result navigation.
- Trade: generated ship marker. Research, alerts, status feedback and shared controls audited.
- 52 icon components: Coin, Check, Information, Warning, Error, Loading, Decoration, Arrow, Fish, Bread, Cursor, Planks, Building, Road, Harbor, Production, Storage, Civic, Farm, Mill, Beer, Trend, Bakery, People, Laborer, Pause, Play, Wealthy, Fast, Grain, Fastest, Flour, Timber, Salt, Iron, Close, Tools, Pin, Search, Minus, Star, Plus, Down, Lock, Back, Settings, Up, Research, Eye, Save, Map, Ship.
- Two portrait components: production worker and residence citizen.
- States: default artwork; hover/pressed/selected/disabled/focus via native surfaces; loading/warning/error/success via generated pictograms and readable labels/tooltips.

## Assets, dimensions and provenance

Built-in ImageGen, one generation per component; no API/CLI provider or hand-drawn substitute. Requested icon size 32x32 and portrait 112x112, or closest supported native canvas. The generator returned larger canvases: most 1254x1254, other exact dimensions are recorded per asset. Originals are preserved without resampling. Approved resizing uses premultiplied-alpha Lanczos with aspect preserved. Faint stray alpha is excluded from margin bounds; antialiased silhouette edges are retained.

- Selected icon masters and sibling final prompts: SourceArt/UI/Icons/icons--*.
- Full final prompt set, exact native dimensions, crop and every output path: [manifest](../../../SourceArt/UI/Icons/manifest.json).
- Runtime icons: Content/Hansa/UI/Icons/NAME--SIZE.png; 676 files at 16,20,24,28,32,40,48,56,64,80,96,112,160px.
- New portrait masters/prompts: SourceArt/UI/Production/production--worker-portrait--default--1254x1254--v2.png and SourceArt/UI/Residence/residence--citizen-portrait--default--1254x1254--v2.png.
- Runtime portraits: Content/Hansa/UI/Production/worker--SIZE.png and Content/Hansa/UI/Residence/citizen--SIZE.png; 56,80,112,160px. Exact source records in SourceArt/UI/Icons/portraits.json.
- Runtime PNGs are integrated through Slate dynamic image brushes and explicitly staged as UFS dependencies. They are shipping artwork, not full-screen references or converted SVGs.
- Review sheets and native screen captures: Docs/Images/UI/AllMenus/Review/. These are non-shipping review evidence.
- Scripts/PrepareGuiImageGenAssets.py reproduces icon variants from repository masters.

## Inspection and corrections

Inspected each selected original and all 52 icons at 16/20/24/32/48 actual pixels on linen and navy. Rejected five early fake-checkerboard outputs and seven opaque/glowing outputs. Regenerated them with genuine alpha. Regenerated the anchor in bright brass after dark-background review. Removed transparent-margin noise from sizing bounds. Replaced two opaque portraits with new transparent ImageGen portraits. Native inspector capture exposed two missing resource mappings; corrected to the generated coin and laborer.

Accepted final silhouettes, aspect ratios, alpha edges, small-size legibility and family consistency. Native screenshots confirm generated goods, controls, status icons and portraits render without placeholder squares or rectangular portrait backgrounds.

## Verification

- HansaEditor Win64 DebugGame build: passed.
- HansaEditor Win64 Development build: passed.
- Hansa.UI.GeneratedIcons.Coverage: passed; validates every enum/density resource, PNG dimensions and RGBA.
- Hansa.UI.GuiRepair.RealViewport at 1920x1080: passed; nine screens in normal and accessible modes (18 captures). Repeated after inspector fixes.
- Hansa.UI.Frontend.RealViewport at 1280x720: passed; 25 title/settings/new-game/pause/save/load/confirmation stages.
- Hansa.UI.TopMenu.RealViewport at 1280x720: passed; 80%,100%,140% scale, geometry, native keyboard speed action and explanatory tooltips.
- Hansa.UI.ResidenceInspector.RealViewport at 1280x720 using the rebuilt DebugGame binary: passed; five states including accessibility.
- Runtime staging declarations inspected. A packaged Shipping executable was not built in this artwork task.

The user-approved GUI resizing exception is recorded in the design brief, asset workflow and repository instructions. The original three-panel layout is preserved. This pass replaces artwork; it is not a redesign of menu layout or world scenery.
