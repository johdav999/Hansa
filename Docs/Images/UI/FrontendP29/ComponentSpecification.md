# P29 frontend and system-screen components

All shipping surfaces are native Slate. Generated images are reference-only, never a full-screen interactive texture.

| Component | Implementation and states |
| --- | --- |
| Title shell/navigation | Native navy backdrop, linen bounded menu, Hansa serif wordmark; Continue enabled only for compatible slots, New game, Load game, Settings, Credits, Quit; default/hover/pressed/disabled/focus/loading/error |
| Session/system navigation | Native Back, return-to-title confirmation, paused gameplay, opener focus restoration |
| Save-slot card/list | Native manual/autosave metadata, named manual save, compatible/empty/corrupt/incompatible/selected/focus; persistent save/load controls |
| Settings row | Native labeled actions/steppers, display/window/VSync, master audio, camera sensitivity, existing UI scale/contrast/text/motion; applied/revert/disabled/focus |
| Confirmation/loading feedback | Native decision card, explicit unsaved-state consequences, cancel/confirm, operation text, actionable recovery; display changes revert unless confirmed |
| Credits/legal | Native scrollable text, verified project font attribution and clearly labeled credits/legal placeholder |
| Charts/icons/decoration | Shared native glyphs and progress feedback; no new chart or shipping raster decoration |

Palette: Baltic Navy #152A35, Harbor Slate #29424D, Linen #F2E9D8, Parchment #DFCFAF, Ink #202628, Muted Ink #596160, Brass #C19A52, Chalk #FAF7EF; oxblood for destructive decisions only. Shared serif headings and sans-serif body, 8px spacing scale, 48px physical controller targets, wrapping localization text. Native geometry scales; raster references never resample.

Generate a composed title reference at 1536x1024, then separate navigation, save-slot, settings-row and confirmation references, one call per component. Reuse P28 dossier/coach and shared P21 controls. Exact prompts and native dimensions are preserved beside each selected reference.
