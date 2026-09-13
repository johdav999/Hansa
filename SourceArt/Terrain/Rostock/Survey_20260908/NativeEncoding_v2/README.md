# Rostock native height encoding revision 2

Staged survey source with native verification evidence. This is not an assembled
city, historical reconstruction, or production acceptance.

The measured `rostock-survey.tif` is an unchanged copy of the parent survey TIFF.
Original LAiV DGM1 download, access metadata, CC BY 4.0 attribution (GeoBasis-DE/M-V),
and the earlier encoded draft are preserved in the parent directory. Horizontal
CRS is EPSG:25833; vertical datum is DHHN2016 normal height (EPSG:7837). Tile-specific
acquisition date remains unresolved.

Unreal 5.8 `LandscapeDataAccess::GetLocalHeight` decodes `(code - 32768) / 128`.
For the declared Z scale 25 and actor Z 3200 cm, use a nominal 65536-step interval,
not a normalized 65535 divisor. Code 65535 represents 95.998046875 m, not 96 m.

`Scripts/ReencodeRostockSurvey.py` corrected encoding in this sibling without
resampling or sculpting the measured raster. 1,205,255 codes changed, with maximum
full-grid encoding error reduced from 0.0019683837890625 m to
0.00096893310546875 m. Heightmap SHA-256:
`879a56a00c635f26e783bc0158db45dbfb06caa35e694752d24f14ef6d1d99b6`.

`survey-preflight.json` records five control points, all-grid encoding error,
native transform and dimensions. The importer pins the exact manifest and R16;
changing either requires a reviewed importer revision. No production content or
the user's Lübeck terrain was overwritten.

Native import/save/reopen and complete measured-layer readback now pass, with
256 Landscape components. Two corrected native guard tests and nine offline tests
pass. `NativeReview_20260908` retains receipts and the inspected technical overview.
The first guard-test fixture inadvertently invoked the authorized staged import;
the concrete replacement verifies fixture dirtiness before invoking the importer.
No existing map was overwritten. See the city-assembly ledger for the full record.

Outstanding: historical
shoreline review; materials and native Water; construction/trade presentation
integration; collision, streaming, visual QA, performance and Shipping proof.
