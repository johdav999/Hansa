"""Small offline fixtures: no Unreal process, providers, or live survey downloads."""
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'Scripts'))
from HansaTerrainContract import ContractError, decode_height, validate


class TerrainContractTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / 'terrain-manifest.json'
        self.payload = struct.pack('<64H', *([32768] * 64))
        (self.root / 'survey.r16').write_bytes(self.payload)
        self.data = dict(schema_version=1, city_id='City.Rostock', source_crs='EPSG:25833',
                         unreal_axes='X east; Y south; Z up', width_vertices=8, height_vertices=8,
                         metres_per_vertex=2, component_quads=7, components_x=1, components_y=1,
                         landscape_xy_scale_cm=200, landscape_z_scale=25, landscape_actor_z_cm=3200,
                         origin_projected_m=[100, 200, 0], bounds_projected_m=[100.5, 186.5, 114.5, 200.5],
                         encoded_min_elevation_m=-32, encoded_max_elevation_m=96, nodata_count=0,
                         heightmap_file='survey.r16', heightmap_sha256=hashlib.sha256(self.payload).hexdigest(),
                         control_points=[dict(row=r,column=c,easting=100.5+c*2,northing=200.5-r*2,height_m=32)
                                         for r,c in ((0,0),(0,7),(7,0),(7,7))])

    def check(self, data=None):
        self.path.write_text(json.dumps(self.data if data is None else data))
        return validate(self.path)

    def test_native_decode_endpoints(self):
        self.assertEqual(decode_height(0,25,3200), -32)
        self.assertEqual(decode_height(32768,25,3200), 32)
        self.assertEqual(decode_height(65535,25,3200), 95.998046875)

    def test_pass_is_not_promotion(self):
        result = self.check()
        self.assertFalse(result['production_accepted'])
        self.assertEqual(result['landscape_origin_cm'], [50,-50,3200])
        self.assertEqual(result['control_points'][2]['unreal_cm'], [50,1350,3200])

    def test_legacy_lubeck_fields(self):
        data = copy.deepcopy(self.data)
        data['city_id'] = 'City.Lubeck'
        data['row_order'] = 'north to south'
        del data['unreal_axes']
        for new,old in [('control_points','control_samples'), ('nodata_count','missing_vertices'),
                        ('encoded_min_elevation_m','encoded_nominal_min_elevation_m'),
                        ('encoded_max_elevation_m','encoded_nominal_max_elevation_m')]:
            data[old] = data.pop(new)
        for point in data['control_samples']:
            point['elevation_m'] = point.pop('height_m')
        self.assertEqual(self.check(data)['survey_preflight'], 'passed')

    def test_bad_scalar_contracts(self):
        for key,value in [('width_vertices',9),('width_vertices',8.5),('width_vertices',True),
                          ('metres_per_vertex',float('nan')),('landscape_z_scale',0),
                          ('landscape_actor_z_cm',0),('landscape_xy_scale_cm',100),
                          ('nodata_count',1),('source_crs','unknown'),('flip_y',True),
                          ('schema_version',2),('component_quads',8)]:
            with self.subTest(key=key,value=value), self.assertRaises(ContractError):
                self.check(dict(self.data, **{key:value}))

    def test_bytes_and_hash(self):
        for payload in [self.payload[:-2], b'\0\0'+self.payload[2:]]:
            (self.root / 'survey.r16').write_bytes(payload)
            with self.assertRaises(ContractError):
                self.check()

    def test_path_escape(self):
        for name in ['C:\\outside.r16', '/outside.r16', '\\\\server\\share\\outside.r16']:
            with self.subTest(name=name), self.assertRaises(ContractError):
                self.check(dict(self.data, heightmap_file=name))
        folder = self.root / 'nested'
        folder.mkdir()
        self.path = folder / 'manifest.json'
        with self.assertRaises(ContractError):
            self.check(dict(self.data, heightmap_file='../survey.r16'))

    def test_grid_orientation_and_native_bias(self):
        for key,value in [('northing',199.5),('easting',102.5),('height_m',32.001953125),('row',-1)]:
            data = copy.deepcopy(self.data)
            data['control_points'][0][key] = value
            with self.subTest(key=key), self.assertRaises(ContractError):
                self.check(data)

    def test_duplicate_and_collinear_controls(self):
        data = copy.deepcopy(self.data)
        data['control_points'][2] = data['control_points'][0]
        with self.assertRaises(ContractError):
            self.check(data)
        data['control_points'] = [dict(row=0,column=c,easting=100.5+c*2,northing=200.5,height_m=32)
                                  for c in (0,1,2)]
        with self.assertRaises(ContractError):
            self.check(data)

    def test_conflicting_aliases_fail(self):
        with self.assertRaises(ContractError):
            self.check(dict(self.data, encoded_nominal_min_elevation_m=-30))


if __name__ == '__main__':
    unittest.main()
