# EMVP-P11 — Coastal Fishery audit

Status: audit started; no new model, texture, Unreal import or production binding yet.

2026-09-08: the user approved P11 promotion. No staged Fishery candidate exists, so
there is nothing to promote yet. This records the request, not approval of an unseen
future asset or completion of the placement work below.

## Existing content and required roles

The P02 manifest identifies `Presentation.Building.Fishery`,
`Presentation.Prop.Fishery.Work` (landing/nets/racks), and
`Presentation.Prop.Fishery.Cargo` (fish baskets/crates). The building remains an Engine
placeholder and the props are missing. A repository filename audit found the Fishery
definition/recipe/good/need assets but no existing fishery model source or mesh family.
No small fishing craft is required by the current manifest, so P11 should not add one.

## Placement issue to resolve before final geometry

`FHansaPlacementRules` rejects Water cells inside any building footprint. A shoreline
building must occupy at least one Shore cell and have Water on one exterior edge with
Land on the opposite edge. Rotation currently swaps footprint dimensions but does not
require the authored water-facing side to face that exterior Water edge. Consequently
a 180-degree rotation may remain valid while an asymmetric landing faces inland.
P11 must reconcile this presentation/placement contract and add directional regression
coverage before claiming valid/invalid shore-placement acceptance. Do not silently
change all shore-building semantics or place collision into unowned water cells.

The Fishery is 3x2 cells: 12x8 m nominal, 11.6x7.6 m grounded inset, identity scale,
ground-centred pivot, and the P07 25/65/120 m camera/LOD/material checks apply.

## Research leads, not accepted photographic evidence

- Lübeck municipal cultural-landscape account: Gothmund developed from a Trave-side
  shelter for city fishermen; its shore-oriented sheds and landings are useful functional
  analogues. Existing houses are not automatically medieval reconstructions.
  https://abh.luebeck.de/de/stadtleben/freizeit/natur-erleben/erholung-naturerleben/kulturlandschaft/index.html
- Warnemünde Heimatmuseum occupies a fisher/seafarer house built in 1767. This is a
  regional work/space reference, not evidence for a late-medieval building's exact form.
  https://heimatmuseum-warnemuende.de/presse/

These were discovered on 2026-09-07; actual reference photographs have not yet been
inspected. No historical reconstruction or material appearance has been accepted.

## Parallel P10 approval

The user approved the reviewed Bakery promotion during this audit. Its first application
saved the Grain Farm metadata repair but Bakery remained file-locked; after the user closed
the editor, the retry saved Bakery and the runtime catalog advanced to v4. See
`EnhancedMvpBakeryAsset.md`. That approval is specific to the reviewed bread-chain diff;
it is not approval of a future, unseen Fishery candidate.
