"""Offline, read-only survey preflight. A pass is NOT terrain promotion approval.

Uses UE LandscapeDataAccess::GetLocalHeight, not a normalized 0..65535 decoder.
Supports the two existing survey manifests without rewriting their provenance.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path, PureWindowsPath
import re
import struct


class ContractError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise ContractError(message)


def number(value, name, integer=False):
    require(type(value) in (int, float) and math.isfinite(value), name + ': finite number required')
    require(not integer or int(value) == value, name + ': integer required')
    return int(value) if integer else float(value)


def field(data, primary, legacy=None):
    if primary in data and legacy in data:
        require(data[primary] == data[legacy], 'Conflicting fields: ' + primary)
    key = primary if primary in data else legacy
    require(key in data, 'Missing field: ' + primary)
    return data[key]


def decode_height(code, z_scale, actor_z_cm):
    return ((code - 32768) / 128.0 * z_scale + actor_z_cm) / 100.0


def validate(path):
    path = Path(path).resolve(strict=True)
    data = json.loads(path.read_text(encoding='utf-8-sig'))
    require(data.get('schema_version') == 1, 'Unsupported survey schema')
    require(data.get('city_id') in ('City.Lubeck', 'City.Rostock'), 'Unsupported city')
    require(re.fullmatch(r'EPSG:[1-9][0-9]+', data.get('source_crs', '')), 'Explicit projected EPSG required')
    require(data.get('row_order') == 'north to south' or data.get('unreal_axes') == 'X east; Y south; Z up',
            'Explicit north-to-south raster / east-south-up axes required')
    require(data.get('flip_y', False) is False, 'Row flips require a separately verified contract')
    width = number(data['width_vertices'], 'width', True)
    height = number(data['height_vertices'], 'height', True)
    quads = number(data['component_quads'], 'component_quads', True)
    cx = number(data['components_x'], 'components_x', True)
    cy = number(data['components_y'], 'components_y', True)
    require(quads in {7, 15, 31, 63, 127, 255, 14, 30, 62, 126, 254, 510}, 'Invalid component topology')
    require(cx > 0 and cy > 0 and cx * cy <= 1024, 'Component budget exceeded')
    require(width == cx * quads + 1 and height == cy * quads + 1, 'Vertex/component mismatch')
    require(width * height <= 8129 ** 2, 'Heightmap sample budget exceeded')
    spacing = number(data['metres_per_vertex'], 'metres_per_vertex')
    xy_scale = number(data['landscape_xy_scale_cm'], 'xy_scale')
    z_scale = number(data['landscape_z_scale'], 'z_scale')
    actor_z = number(data['landscape_actor_z_cm'], 'actor_z')
    require(spacing > 0 and z_scale > 0 and math.isclose(xy_scale, spacing * 100, abs_tol=1e-7), 'Invalid physical scale')
    bounds = [number(v, 'bounds') for v in data['bounds_projected_m']]
    origin = [number(v, 'origin') for v in data['origin_projected_m']]
    require(len(bounds) == 4 and len(origin) == 3, 'Invalid bounds/origin dimensions')
    require(all(v == int(v) for v in origin), 'GeoReferencing origin must be integral metres')
    west, south, east, north = bounds
    require(math.isclose(east-west, (width-1)*spacing, abs_tol=1e-6) and
            math.isclose(north-south, (height-1)*spacing, abs_tol=1e-6), 'Bounds do not match vertex grid')
    low = number(field(data, 'encoded_min_elevation_m', 'encoded_nominal_min_elevation_m'), 'encoding minimum')
    high = number(field(data, 'encoded_max_elevation_m', 'encoded_nominal_max_elevation_m'), 'encoding maximum')
    require(high > low and math.isclose(z_scale, (high-low)*100/512, abs_tol=1e-7) and
            math.isclose(actor_z, (high+low)*50, abs_tol=1e-7), 'Encoding/Unreal transform mismatch')
    require(number(field(data, 'nodata_count', 'missing_vertices'), 'NoData count', True) == 0, 'Unresolved NoData')
    relative = data['heightmap_file']
    require(isinstance(relative, str) and relative and not PureWindowsPath(relative).drive and
            not PureWindowsPath(relative).root and not Path(relative).is_absolute(), 'Relative source path required')
    source = (path.parent / relative).resolve(strict=True)
    require(source.is_relative_to(path.parent) and source.is_file(), 'Heightmap escapes survey package')
    require(source.stat().st_size == width*height*2, 'R16 byte count mismatch')
    payload = source.read_bytes()
    digest = hashlib.sha256(payload).hexdigest()
    require(digest == data['heightmap_sha256'], 'Heightmap SHA-256 mismatch')
    controls = field(data, 'control_points', 'control_samples')
    require(isinstance(controls, list) and len(controls) >= 3, 'At least three control points required')
    coordinates = set()
    results = []
    # A half native quantization step plus floating-point source tolerance.
    tolerance = z_scale / 25600.0 + 0.00001
    for point in controls:
        row = number(point['row'], 'control row', True)
        column = number(point['column'], 'control column', True)
        require(0 <= row < height and 0 <= column < width, 'Control outside grid')
        require((row, column) not in coordinates, 'Duplicate control point')
        coordinates.add((row, column))
        easting = number(point['easting'], 'control easting')
        northing = number(point['northing'], 'control northing')
        require(abs(easting-(west+column*spacing)) < 1e-6 and
                abs(northing-(north-row*spacing)) < 1e-6, 'Control orientation/grid mismatch')
        measured = number(field(point, 'height_m', 'elevation_m'), 'control elevation')
        code = struct.unpack_from('<H', payload, (row*width+column)*2)[0]
        decoded = decode_height(code, z_scale, actor_z)
        require(abs(decoded-measured) <= tolerance,
                f'Native height error at ({row},{column}): {abs(decoded-measured):.9f} m > {tolerance:.9f} m')
        results.append({'row': row, 'column': column, 'source_m': measured,
                        'native_decoded_m': decoded, 'error_m': decoded-measured,
                        'unreal_cm': [(easting-origin[0])*100, (origin[1]-northing)*100, decoded*100]})
    points = list(coordinates)
    a, b = points[:2]
    require(any((b[0]-a[0])*(p[1]-a[1]) != (b[1]-a[1])*(p[0]-a[0]) for p in points[2:]),
            'Control points must be non-collinear')
    return {'city_id': data['city_id'], 'survey_preflight': 'passed', 'production_accepted': False,
            'heightmap_sha256': digest, 'control_tolerance_m': tolerance, 'control_points': results,
            'landscape_origin_cm': [(west-origin[0])*100, (origin[1]-north)*100, actor_z],
            'components': [cx, cy], 'native_height_range_m': [decode_height(0,z_scale,actor_z), decode_height(65535,z_scale,actor_z)],
            'remaining': ['Native import/reopen and control-point comparison', 'License/datum/acquisition review',
                          'Historical hydrology and gameplay grading', 'Materials/water/streaming/collision/Shipping acceptance']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(validate(args.manifest), indent=2))
    except (ContractError, KeyError, TypeError, OSError, json.JSONDecodeError) as error:
        parser.exit(1, 'SURVEY_PREFLIGHT_FAILED: ' + str(error) + '\n')
