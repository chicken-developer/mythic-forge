"""Regression checks for destructive-generation preconditions and reproducibility."""
import copy, importlib.util, json, unittest, collections
from pathlib import Path
spec=importlib.util.spec_from_file_location('prepare',Path(__file__).with_name('prepare.py'))
p=importlib.util.module_from_spec(spec); spec.loader.exec_module(p)

class DesignTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.d=json.loads((p.BASE/'layout.json').read_text(encoding='utf-8-sig'))
    def test_deterministic_ids_and_massing(self):
        a=p.Compiler(copy.deepcopy(self.d)).compile(); b=p.Compiler(copy.deepcopy(self.d)).compile()
        self.assertEqual(a.actors,b.actors); self.assertEqual(a.meshes,b.meshes)
        self.assertEqual(len(a.ids),len(a.actors)); self.assertFalse(a.validate()['errors'])
    def test_swapped_architecture_rejected(self):
        d=copy.deepcopy(self.d); d['architecture_regions'][0]['architecture_style']='Gothic'
        self.assertTrue(any('Architecture mapping' in e for e in p.Compiler(d).compile().validate()['errors']))
    def test_invalid_geometry_and_landscape_rejected(self):
        for mutate in [lambda d:d['roads'][0]['points_m'].__setitem__(1,d['roads'][0]['points_m'][0]),
                       lambda d:d['heightmap'].__setitem__('resolution',1000),
                       lambda d:d.__setitem__('scale',0),
                       lambda d:d['land'][0].__setitem__('polygon_m',[[0,0],[10,10],[0,10],[10,0]])]:
            d=copy.deepcopy(self.d); mutate(d)
            with self.assertRaises(ValueError): p.input_check(d)
    def test_unsafe_ramp_and_disconnected_branch_rejected(self):
        d=copy.deepcopy(self.d); d['roads'][0]['points_m'][2][2]=100
        self.assertTrue(any('Ramp' in e for e in p.Compiler(d).compile().validate()['errors']))
        d=copy.deepcopy(self.d); branch=next(r for r in d['roads'] if r['id']=='residential')
        for q in branch['points_m']: q[0]-=1000
        self.assertTrue(any('Disconnected route residential' in e for e in p.Compiler(d).compile().validate()['errors']))
    def test_building_footprints_stay_in_architecture_polygon(self):
        c=p.Compiler(copy.deepcopy(self.d)).compile(); regions={r['id']:r for r in self.d['architecture_regions']}
        for b in c.buildings:
            if b['landmark']: continue
            region=next(r for id,r in regions.items() if b['id'].startswith(id+'_'))
            x,y,z=b['center_m']; w,h=b['size_m'][:2]
            self.assertTrue(all(p.inside(q,region['polygon_m']) for q in [[x-w/2,y-h/2],[x+w/2,y-h/2],[x+w/2,y+h/2],[x-w/2,y+h/2]]))
            self.assertFalse(c.reserved(x,y,w,h)); self.assertEqual(b['style'],region['architecture_style'])

    def test_road_and_stair_collision_shells_are_closed_and_consistently_wound(self):
        c=p.Compiler(copy.deepcopy(self.d)).compile()
        for route in self.d['roads']+self.d['stairs']:
            mesh=c.meshes[route['id']]; edges=collections.Counter(); faces=set()
            for tri in mesh['triangles']:
                self.assertNotIn(tuple(sorted(tri)),faces); faces.add(tuple(sorted(tri)))
                for a,b in zip(tri,tri[1:]+tri[:1]): edges[a,b]+=1
            self.assertTrue(all(count==1 and edges[b,a]==1 for (a,b),count in edges.items()),route['id'])

    def test_invalid_metric_parameters_rejected(self):
        for key in ('foundation_cap_m','retaining_cap_m','terminal_extension_m','surface_tolerance_m'):
            d=copy.deepcopy(self.d); d['geometry'][key]=0
            with self.assertRaises(ValueError): p.input_check(d)
        d=copy.deepcopy(self.d); d['dimensions']['MinorGridSizeCm']=100
        with self.assertRaises(ValueError): p.input_check(d)

if __name__=='__main__': unittest.main()
