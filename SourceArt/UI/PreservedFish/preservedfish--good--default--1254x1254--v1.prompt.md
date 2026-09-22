# Preserved fish icon

Status: selected master, actual-size GUI QA complete. Built-in ImageGen, new generation; model not exposed. Requested 1024 square, returned 1254 x 1254 RGBA. Intended use: Good.PreservedFish. Original pixels preserved; approved GUI resizing exception applies to later display variants.

## Final prompt
Create one production GUI good icon for the original medieval Hanseatic city-building game Hansa: Preserved fish, an open small oak barrel packed with clearly recognizable silver salted herring, a restrained scattering of salt crystals inside, three-quarter overhead view. Single coherent isolated object, strong readable silhouette at 32 and 48 pixels. Slight engraved painterly realism matching a historical merchant ledger, natural aged oak #795137, restrained brass #C19A52 accents, dark ink #202628 outlines, silver fish, no shiny mobile-game gloss. Genuine transparent RGBA background, no floor or backdrop, no text or label, no UI frame, no watermark, no clipped edges or shadows. Center with 12 percent transparent safe margins. Request native 1024x1024 square master; closest supported square size acceptable. This is one individual icon, not a contact sheet. No other objects.

## Inspection
Original output inspected: recognizable fish-filled oak barrel, no lettering, intact silhouette and margins. RGBA alpha spans 0-255. All thirteen native display variants (16,20,24,28,32,40,48,56,64,80,96,112,160) reviewed at actual pixels on linen and navy in display-size-review-final.png. Silhouette is readable; detailed herring is recognizable from 32px upward. Tiny variants retain the barrel silhouette and require accompanying text. No obvious colored halo or stretched content.

Display variants use proportional Lanczos downsampling under the user-approved GUI exception; the 1254-square master is retained unchanged. Full square source, no content crop. Runtime selects exact-size PNG variants; the review board itself does not resize them.
