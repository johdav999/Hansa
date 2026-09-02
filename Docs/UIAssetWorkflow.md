# Hansa — GUI and Image Asset Workflow

## 1. Purpose

This workflow turns the visual direction in [UIDesignBrief.md](UIDesignBrief.md) into consistent, reusable Unreal Engine UI. It governs both generated visual references and production raster assets.

Its central rule is:

> Design the system, inventory the components, generate at native size, inspect at native size, and assemble the interactive result in UMG/Slate.

Generated screen mockups communicate direction. They are not a substitute for an implemented interface.

## 2. Source-of-truth order

For UI and game-image work, use this order:

1. the user's current explicit request;
2. `Docs/UIDesignBrief.md`;
3. this workflow;
4. an approved screen/component specification;
5. an approved style-anchor image;
6. earlier explorations and rejected variants.

If a higher source changes the design, update lower sources rather than allowing silent divergence.

## 3. Classify the deliverable

Before work begins, label every requested output:

| Class | Purpose | Shipping status |
| --- | --- | --- |
| Screen reference | Explore hierarchy and visual direction | Never shipped as interactive UI |
| Component reference | Define appearance/states for implementation | Recreated natively unless explicitly raster |
| Production raster | Portrait, illustration, texture, painted element | May ship after technical QA/import |
| Vector/SDF reference | Visual source for icon or scalable shape | Recreated in scalable form |
| Native widget | Interactive layout, text, table, chart, control | Implemented in UMG/Slate |
| Style anchor | Consistency reference for an asset family | Stored with project documentation/source art |

Do not call a mockup production-ready when its text, interaction, layout, or data is still baked into a bitmap.

## 4. Screen decomposition

Create a component inventory before any image generation.

### 4.1 Screen shell

- background/world viewport treatment;
- safe-area frame;
- top status bar;
- primary navigation;
- modal/drawer layer;
- notification layer;
- tooltip layer;
- controller focus layer.

### 4.2 Navigation

- tabs;
- breadcrumbs;
- back/close controls;
- screen switcher;
- filters/search;
- time and game-speed controls.

### 4.3 Information surfaces

- inspector panel;
- cards;
- tables and virtualized rows;
- list headers;
- section headers;
- data summaries;
- empty, loading, stale, and error states.

### 4.4 Controls

- primary, secondary, destructive, and icon buttons;
- toggles and checkboxes;
- sliders;
- dropdowns;
- quantity steppers;
- sortable column headers;
- drag handles;
- route-stop and map-node controls.

### 4.5 Feedback

- selected/focus treatment;
- valid/invalid placement;
- notice, warning, critical, and decision alerts;
- progress and cooldown;
- success/rejection response;
- data-age and uncertainty treatment.

### 4.6 Data visualization

- price chart;
- stock/reserve bar;
- production chain node and edge;
- supply/demand factor stack;
- route line and direction marker;
- map city marker;
- capacity and schedule timeline.

### 4.7 Image assets

- good/building/technology icons;
- portraits;
- event illustrations;
- panel ornament;
- material textures;
- map decoration;
- loading and empty-state illustrations.

For every component, state whether it will be native, vector/SDF, material-driven, or raster.

## 5. Component specification

Each component receives a short specification before generation or implementation:

```text
Component ID:
Screen/feature:
Purpose:
Deliverable class:
Implementation: UMG / Slate / vector-SDF / material / raster
Native pixel dimensions:
Aspect ratio:
Transparency:
Safe margins:
States:
Content variations:
Palette tokens:
Typography token:
Interaction/input:
Accessibility requirement:
Localization expansion:
Style anchor:
Avoid:
```

If native dimensions are unknown, the component is not ready for raster generation.

## 6. State matrix

Every interactive component is reviewed against this matrix:

| State | Required treatment |
| --- | --- |
| Default | Resting readable state |
| Hover | Pointer affordance without moving layout |
| Pressed | Immediate tactile confirmation |
| Selected | Persistent selection distinct from hover |
| Disabled | Reduced emphasis plus explanation where useful |
| Keyboard/controller focus | High-contrast focus ring independent of hover |
| Loading | Stable geometry; no layout jump |
| Warning | Amber + icon/shape/text |
| Error/critical | Oxblood + icon/shape/text |

Generate separate raster states only if the visual cannot be produced through native tint, material parameters, overlay, or animation. Never scale one state to imitate another.

## 7. Choosing the right implementation

### Use native UMG/Slate for

- layout and responsive containers;
- all dynamic/localized text;
- buttons, focus, hit targets, and input handling;
- tables and lists;
- graphs and changing economic data;
- progress, stock, reserve, and workforce bars;
- route lines, selection outlines, and placement grids;
- colors or states that must change dynamically.

### Use vector/SDF or native shapes for

- small icons;
- scalable glyphs;
- thin lines and arrows;
- focus and selection shapes;
- route and map symbols;
- monochrome status marks.

ImageGen may establish their visual direction, but approved forms should be recreated in a scalable deterministic format when practical.

### Use raster assets for

- portraits and event illustrations;
- complex painted goods/building imagery;
- historically grounded decorative scenes;
- subtle material texture that cannot be produced efficiently by a UI material;
- large fixed-size decorative components with no dynamic text.

### Use UI materials for

- subtle gradients and noise;
- animated highlights;
- masked fills;
- desaturation and disabled treatment;
- reusable paper, ink, metal, and cloth effects;
- effects that otherwise require many raster states.

## 8. No-resampling policy

### 8.1 Prohibited

- stretching width or height independently;
- resizing a generated raster after creation;
- upscaling a low-resolution component;
- downscaling a large component for normal use;
- using a UMG render transform to compensate for the wrong source size;
- exporting a resized copy from an image editor;
- generating one screen ratio and stretching it to another;
- extracting a component from a montage and enlarging it.

### 8.2 Required alternatives

- generate a dedicated native-size asset;
- generate separate `1x`, `1.5x`, and `2x` density variants;
- generate separate 16:9, 16:10, and ultrawide composition variants where the imagery itself must change;
- tile texture centers instead of stretching them;
- build flat fills, borders, lines, and shadows natively;
- use vector/SDF for scalable icons;
- use protected corners/edges with a procedural or tiled center for framed panels;
- choose a supported generator size as the intended native master rather than resizing it later.

Cropping is allowed only to remove unused transparent margin or create a deliberately specified crop without resampling content. Record the crop in the prompt/revision notes.

### 8.3 Exact-size limitation

The built-in ImageGen path may not expose arbitrary output dimensions. If a required exact native dimension cannot be produced:

1. try a supported native aspect/dimension that can become the component's defined source size;
2. regenerate rather than resize;
3. if exact API-controlled size is essential, ask the user whether to use the explicit GPT Image API/CLI workflow;
4. do not silently fall back to resampling.

## 9. Image generation workflow

### Step 1 — Read and inventory

- Read `UIDesignBrief.md` and this workflow completely.
- Identify the user flow and screen state being designed.
- Produce the component inventory.
- Mark every component as native, scalable, material, reference-only raster, or production raster.

### Step 2 — Establish a style anchor

- Generate one high-fidelity composed screen or component-family anchor.
- Validate palette, material restraint, typography direction, line weight, icon treatment, information density, and historical tone.
- Store the approved anchor under `Docs/Images/UI/<Feature>/`.
- Use that exact anchor as a reference image for later components when supported.

### Step 3 — Generate component references

- Generate each distinct component in a separate call.
- Keep viewpoint, palette, material, border weight, lighting, and negative constraints consistent.
- Request genuine transparency for layered raster components.
- Include generous safe margins so shadows and outlines are not clipped.
- Avoid text unless the text is permanent decorative artwork.
- Do not generate dynamic numbers, labels, shortcuts, or status into production raster.

### Step 4 — Inspect at native resolution

Use the image viewing tool at original detail and check:

- correct subject and component type;
- exact pixel dimensions/aspect ratio;
- clean alpha and edges;
- no clipped shadows;
- no unintended text, logo, or watermark;
- no malformed geometry or icon silhouette;
- correct palette tokens and contrast;
- consistency with the style anchor;
- adequate negative space and safe margins;
- correct state and interaction affordance.

### Step 5 — Iterate narrowly

- Make one targeted correction per iteration.
- State invariants explicitly: what must remain unchanged.
- Save corrected work as a versioned sibling.
- Re-inspect at original resolution.
- Remove superseded project-local drafts after final selection when they have no comparison value; generator originals may remain in their default archive.

### Step 6 — Persist and document

- Copy selected reference images into `Docs/Images/UI/<Feature>/`.
- Copy selected production masters into `SourceArt/UI/<Feature>/`.
- Save a matching `.prompt.md` record.
- Record native dimensions, alpha, source mode, intended Unreal name, and import notes.
- Never leave a referenced project asset only in the generator output directory.

### Step 7 — Implement natively

- Build the screen with UMG/Slate and shared style tokens.
- Import only the individual raster assets that require raster treatment.
- Connect C++ view models and event-driven updates.
- Implement focus, input, localization, responsive layout, and accessibility separately from artwork.

### Step 8 — Assemble and verify

- Compare the implemented screen against the style anchor.
- Verify every component state.
- Test target resolutions and density variants without resampling raster assets.
- Test mouse, keyboard, and controller.
- Run contrast, color-blind, high-contrast, reduced-motion, and large-text checks.
- Test representative German and other long localized labels.

## 10. Prompt template

Use this structure for a new component:

```text
Use case: ui-mockup or stylized-concept
Asset type: Hansa <reference or production component>
Primary request: <one specific component>
Intended use: <screen and role>
Native dimensions/aspect: <required target; never resize>
State: <default/hover/etc.>
Style anchor: <image path/reference role>
Style/medium: historically grounded Hanseatic mercantile UI with modern clarity
Color palette: exact tokens from UIDesignBrief.md
Materials/textures: restrained linen, ink, oak, brass, and brick as applicable
Composition/framing: <silhouette, padding, protected edges>
Transparency: genuine transparent background if layered
Text (verbatim): none unless permanently decorative
Constraints: original design; clean edges; readable silhouette; no resampling planned
Avoid: logos, trademarks, watermark, fantasy ornament, glossy mobile styling, baked dynamic text, clipped shadows
```

For edits, add:

```text
Change only: <one correction>
Keep unchanged: dimensions, composition, palette, materials, alpha, and every unrelated component detail
```

## 11. Prompt record

Create `<asset-basename>.prompt.md` beside every final generated master:

```markdown
# <Asset name>

- Status: reference | production master
- Generator mode: built-in ImageGen | explicit API/CLI
- Model: not exposed | exact model ID
- Native size: WIDTH × HEIGHT
- Transparency: yes/no
- Style anchor: path or none
- Intended Unreal asset: name or reference-only
- Revision: vN and reason

## Final prompt

<verbatim final prompt>

## QA

- [ ] Inspected at original resolution
- [ ] Correct dimensions/aspect
- [ ] Clean alpha/edges
- [ ] Palette and style match
- [ ] No unwanted text/logo/watermark
- [ ] State/safe margin verified
- [ ] No resampling used
```

## 12. Asset paths and naming

### Reference mockups

```text
Docs/Images/UI/<Feature>/
```

Example:

```text
Docs/Images/UI/Market/market-screen--selected-grain--1672x941--v2.png
```

### Source masters

```text
SourceArt/UI/<Feature>/
```

Example:

```text
SourceArt/UI/Market/market--shortage-seal--warning--256x256--v1.png
SourceArt/UI/Market/market--shortage-seal--warning--256x256--v1.prompt.md
```

### Unreal assets

```text
Content/Hansa/UI/<Feature>/
```

Naming:

- textures: `T_UI_<Feature>_<Component>_<State>`;
- widgets: `WBP_<Feature>_<Component>`;
- materials: `M_UI_<Feature>_<Purpose>`;
- material instances: `MI_UI_<Feature>_<Purpose>`;
- fonts: `Font_Hansa_<Family>`;
- data/style assets: `DA_UI_<Purpose>`.

## 13. Unreal import checklist

For production UI raster textures:

- confirm imported dimensions match the master exactly;
- select the UI texture group;
- use the editor's `UserInterface2D (RGBA)` compression profile when appropriate;
- preserve sRGB for color artwork and use the correct color-space treatment for masks/data;
- use clamp addressing for non-tiling components;
- retain alpha without colored fringe;
- avoid automatic power-of-two padding unless required and approved;
- do not create a resized import copy;
- disable mipmaps for strictly fixed 1:1 screen assets; use an explicitly authored density variant when another display size is required;
- record intended native display size in asset metadata or the component spec.

Verify the imported texture visually in UMG at its native size. Engine import success is not visual QA.

## 14. Assembly rules

- Use centralized design tokens, not widget-local approximations.
- Use native text for every changing or localizable label.
- Keep hit targets independent of ornamental image bounds.
- Maintain stable layout between loading, empty, normal, warning, and error states.
- Use native overlays/tints/material parameters for state when they preserve the approved artwork.
- Never distort an asset for a hover or pressed animation; animate opacity, translation, glow, or separate native geometry.
- Ensure panel ornament cannot intercept input.
- Keep charts and market data event-driven and accessible to tooltips/screen-reader equivalents where supported.
- Reuse the same component rather than recreating visually similar copies per screen.

## 15. Review checklist

### Design system

- [ ] `UIDesignBrief.md` was read and followed.
- [ ] Component inventory exists.
- [ ] Palette, type, spacing, and material tokens match.
- [ ] A style anchor was approved and referenced.
- [ ] The result is original and not a copy of another game's UI.

### Components

- [ ] Every distinct component has a defined implementation type.
- [ ] Required states are specified.
- [ ] Dynamic text/data is native, not baked.
- [ ] Focus and disabled states are clear.
- [ ] Localization expansion is supported.

### Images

- [ ] ImageGen was used for requested GUI/image generation.
- [ ] Distinct components were generated separately.
- [ ] Final assets were inspected at original resolution.
- [ ] Native dimensions and aspect are correct.
- [ ] No raster was upscaled, downscaled, stretched, or squashed.
- [ ] Alpha, shadows, and safe margins are clean.
- [ ] Prompts and revision notes are stored beside masters.

### Usability and accessibility

- [ ] Status is not conveyed by color alone.
- [ ] Contrast meets the brief.
- [ ] Mouse, keyboard, and controller flows work.
- [ ] Reduced motion, large text, and high contrast remain usable.
- [ ] Error messages explain cause and remedy.
- [ ] The central 3D play area remains readable where required.

### Unreal implementation

- [ ] Shipping GUI is assembled in UMG/Slate.
- [ ] View models update UI without Blueprint tick polling.
- [ ] Lists/tables are virtualized where necessary.
- [ ] Imported textures retain native dimensions.
- [ ] Raster density/aspect variants are selected rather than resized.
- [ ] Final screen was tested at every supported reference resolution.

## 16. Definition of done

A GUI/image task is complete only when:

1. the component inventory and state matrix are documented;
2. the composed reference and required component references/assets are generated;
3. selected assets are saved inside the repository with prompt records;
4. every selected image passes native-resolution inspection;
5. no raster resampling or stretching was used;
6. the interactive screen is assembled from native UMG/Slate components where implementation was requested;
7. accessibility, input, localization, and resolution checks pass;
8. the completion report distinguishes reference art from production-ready assets.

