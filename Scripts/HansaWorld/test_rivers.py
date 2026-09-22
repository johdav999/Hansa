"""No-network regression checks for the actual exported spline migration."""
import json
import unittest
import rivers
import numpy as np
from scipy.interpolate import CubicHermiteSpline

class RiverProfiles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=json.loads((rivers.OUT/'rivers.json').read_text(encoding='utf-8'))

    def test_source_and_outputs_are_pinned(self):
        self.assertEqual(self.data['source_manifest_sha256'],rivers.sha(rivers.SOURCE/'terrain-manifest.json'))
        for name,digest in self.data['source_hashes'].items():
            self.assertEqual(digest,rivers.sha(rivers.SOURCE/name),name)
        for name,digest in self.data['files'].items():
            self.assertEqual(digest,rivers.sha(rivers.OUT/name),name)

    def test_continuous_downstream_profiles(self):
        ids=set()
        nodes={}
        for river in self.data['rivers']:
            self.assertNotIn(river['id'],ids)
            ids.add(river['id'])
            p=np.array(river['points'])
            self.assertTrue(np.isfinite(p).all())
            self.assertTrue((p[:,3]>=.05).all() and (p[:,3]<=40).all())
            if river['name']=='Trave':
                curve=CubicHermiteSpline(np.arange(len(p)),p[:,:2],p[:,5:7])
                keys=np.linspace(0,len(p)-1,len(p)*128)
                v,a=curve(keys,1),curve(keys,2)
                curvature=np.abs(v[:,0]*a[:,1]-v[:,1]*a[:,0])/np.maximum(np.linalg.norm(v,axis=1)**3,1e-12)
                half_width=np.interp(keys,np.arange(len(p)),p[:,3])/2
                self.assertLess(float(np.max(curvature*half_width)),.8,'Inner bank must not fold')
            spline=CubicHermiteSpline(np.arange(len(p)),p[:,2],p[:,7])
            dense=spline(np.linspace(0,len(p)-1,20*len(p)))
            self.assertLessEqual(float(np.max(np.diff(dense))),.00001,river['name'])
            for point in (p[0],p[-1]):
                key=tuple(np.rint(point[:2]*10).astype(int))
                if key in nodes:self.assertAlmostEqual(nodes[key],point[2],places=5)
                nodes[key]=point[2]
        self.assertGreater(len(ids),100)

    def test_dry_city_cores_are_unchanged(self):
        m=json.loads((rivers.SOURCE/'terrain-manifest.json').read_text(encoding='utf-8'))
        w,h=m['vertices']; step=m['world_step_m']
        delta=np.fromfile(rivers.OUT/'riverbed-delta.r16',dtype='<u2').reshape(h,w)
        water=np.load(rivers.SOURCE/'water-surface.npy')
        for city in m['cities']:
            cx,cy=city['x_cm']/100/step,city['y_cm']/100/step
            r=city['core_radius_m']/step
            x0,x1=max(0,int(cx-r)-1),min(w,int(cx+r)+2)
            y0,y1=max(0,int(cy-r)-1),min(h,int(cy+r)+2)
            yy,xx=np.mgrid[y0:y1,x0:x1]
            mask=(np.hypot(xx-cx,yy-cy)<r)&~np.isfinite(water[y0:y1,x0:x1])
            self.assertTrue((delta[y0:y1,x0:x1][mask]==32768).all(),city['name'])

    def test_migration_keeps_lakes_separate(self):
        cells=np.fromfile(rivers.OUT/'lake-cells.f32',dtype='<f4').reshape(-1,3)
        self.assertEqual(len(cells),self.data['lake_cells'])
        self.assertGreater(self.data['removed_river_cells'],1000)
        self.assertTrue(np.isfinite(cells).all())

    def test_approved_sea_cutouts(self):
        directory=rivers.OUT/'SeaCorridors_v1'
        manifest=json.loads((directory/'sea-corridors.json').read_text(encoding='utf-8'))
        self.assertEqual(manifest['river_sha256'],rivers.sha(rivers.OUT/'rivers.json'))
        for name,digest in manifest['source_hashes'].items():
            self.assertEqual(digest,rivers.sha(rivers.SOURCE/name))
        labels=set()
        for tile in manifest['tiles']:
            self.assertNotIn(tile['actor'],labels)
            labels.add(tile['actor'])
            self.assertRegex(tile['actor'],r'^SM_Sea_0[0-7]_0[0-5]$')
            self.assertEqual(tile['sha256'],rivers.sha(directory/tile['file']))
            triangles=np.fromfile(directory/tile['file'],dtype='<f4').reshape(-1,3,3)
            self.assertTrue(np.isfinite(triangles).all())
            self.assertTrue((triangles[:,:,2]==0).all())
            self.assertEqual(triangles.size//3,tile['vertices'])
        self.assertGreater(len(labels),0)
        self.assertLessEqual(len(labels),48)

if __name__=='__main__':
    unittest.main()
