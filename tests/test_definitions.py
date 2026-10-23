import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from generate_plugin import validate, generate


class Definitions(unittest.TestCase):
    def setUp(self):
        self.d = json.loads((ROOT/'definitions/LayerControl.json').read_text())
        self.abi = json.loads((ROOT/'definitions/abi.json').read_text())

    def test_production_definitions_and_generated_headers(self):
        for name in ('LayerControl','LayerMaskControl'):
            generate(ROOT/f'definitions/{name}.json',ROOT/f'source/generated/{name}.h',ROOT/'definitions/abi.json',True)

    def test_invalid_definitions(self):
        changes = [lambda d: d['parameters'][1].update(id=0),
                   lambda d: d['parameters'][2].update(key='enabled'),
                   lambda d: d['parameters'][2].update(default=float('nan')),
                   lambda d: d['parameters'][2].update(property='/parameter/by-id/123'),
                   lambda d: d['parameters'][2].update(tolerance=-1),
                   lambda d: d['parameters'][3].update(options=['Alpha','Alpha']),
                   lambda d: d['parameters'][3].update(default=999),
                   lambda d: d.update(pluginId='RLCW'),
                   lambda d: d['parameters'][0].update(default=2),
                   lambda d: d['parameters'][1].update(min=0),
                   lambda d: d['parameters'][2].update(unknown=True)]
        for change in changes:
            with self.subTest(change=change):
                d=copy.deepcopy(self.d); change(d)
                with self.assertRaises(ValueError): validate(d,self.abi)

    def test_abi_rejects_reinterpretation(self):
        for field,value in [('type','integer'),('property','maskmode'),('max',20)]:
            d=copy.deepcopy(self.d); d['parameters'][2][field]=value
            with self.assertRaises(ValueError): validate(d,self.abi)
        d=copy.deepcopy(self.d); d['parameters'][3]['options'].reverse()
        with self.assertRaises(ValueError): validate(d,self.abi)

    def test_retirement_and_extension(self):
        d=copy.deepcopy(self.d)
        d['parameters'][2].pop('property');d['parameters'][2]['role']='reserved'
        d['parameters'].append(dict(id=5,key='new_number',label='New Number',type='number',default=0,min=0,max=1,property='test.value'))
        validate(d,self.abi)

    def test_stale_generation_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            output=Path(folder)/'generated.h';output.write_text('stale')
            with self.assertRaises(ValueError):
                generate(ROOT/'definitions/LayerControl.json',output,ROOT/'definitions/abi.json',True)


if __name__ == '__main__': unittest.main()
