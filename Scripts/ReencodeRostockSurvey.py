"""Correct the draft's normalized R16 encoding in a sibling; never alter measured data."""
import hashlib
import json
from pathlib import Path
import shutil
import sys

from HansaTerrainContract import validate

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / 'Saved/GenerationJobs/LubeckTerrain_20260907/python-deps'))
import numpy as np
import rasterio


def main():
    original = REPO / 'SourceArt/Terrain/Rostock/Survey_20260908'
    destination = original / 'NativeEncoding_v2'
    if destination.exists():
        raise RuntimeError('Revision destination exists; inspect it before retrying')
    manifest = json.loads((original / 'terrain-manifest.json').read_text())
    with rasterio.open(original / 'rostock-survey.tif') as raster:
        grid = raster.read(1).astype(np.float64)
        assert grid.shape == (2017,2017) and raster.crs.to_epsg() == 25833
        assert np.isfinite(grid).all()
        if raster.nodata is not None:
            assert not np.any(grid == raster.nodata)
    previous = np.fromfile(original / manifest['heightmap_file'], dtype='<u2').reshape(grid.shape)
    z_scale = manifest['landscape_z_scale']
    actor_z = manifest['landscape_actor_z_cm']
    unrounded = (grid*100-actor_z)/z_scale*128+32768
    codes = np.rint(unrounded)
    assert codes.min() >= 0 and codes.max() <= 65535
    codes = codes.astype('<u2')
    decoded = ((codes.astype(np.float64)-32768)/128*z_scale+actor_z)/100
    previous_decoded = ((previous.astype(np.float64)-32768)/128*z_scale+actor_z)/100
    error = float(np.abs(decoded-grid).max())
    assert error <= z_scale/25600 + 1e-8
    destination.mkdir()
    # Unmodified measured TIFF, not a resampled image or an artistic terrain edit.
    shutil.copy2(original / 'rostock-survey.tif', destination / 'rostock-survey.tif')
    encoded = destination / manifest['heightmap_file']
    codes.tofile(encoded)
    manifest['encoding'] = 'UE LandscapeDataAccess: (code-32768)/128 * z_scale + actor_z_cm; little endian uint16'
    manifest['encoding_revision'] = 2
    manifest['previous_heightmap_sha256'] = manifest['heightmap_sha256']
    manifest['heightmap_sha256'] = hashlib.sha256(encoded.read_bytes()).hexdigest()
    manifest['maximum_encoding_error_m'] = error
    manifest['previous_native_decoding_maximum_error_m'] = float(np.abs(previous_decoded-grid).max())
    manifest['changed_codes'] = int(np.count_nonzero(codes != previous))
    manifest['original_source_package'] = '..'
    manifest['measured_tiff_sha256'] = hashlib.sha256((destination/'rostock-survey.tif').read_bytes()).hexdigest()
    output = destination / 'terrain-manifest.json'
    output.write_text(json.dumps(manifest, indent=2))
    report = validate(output)
    report['full_grid_encoding_error_m'] = error
    report['previous_full_grid_encoding_error_m'] = manifest['previous_native_decoding_maximum_error_m']
    report['changed_codes'] = manifest['changed_codes']
    (destination / 'survey-preflight.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
