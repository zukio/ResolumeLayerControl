"""Validate declarative FFGL definitions and generate deterministic embedded headers."""
import argparse
import json
import math
from pathlib import Path
import re


def require(condition, message):
    if not condition:
        raise ValueError(message)


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def validate(d, abi):
    require(set(d) == {'schemaVersion', 'symbol', 'pluginId', 'name', 'parameters'}, 'unknown/missing definition field')
    require(d['schemaVersion'] == 1, 'unsupported schemaVersion')
    require(re.fullmatch(r'[A-Za-z][A-Za-z0-9]*', d['symbol']), 'invalid symbol')
    require(re.fullmatch(r'[A-Z0-9]{4}', d['pluginId']), 'pluginId must be four ASCII characters')
    require(d['pluginId'] not in ('RLCW', 'RLCP'), 'legacy plugin ID is reserved')
    require(isinstance(d['name'], str) and 0 < len(d['name'].encode('ascii')) <= 32, 'invalid plugin name')
    params = d['parameters']
    require(isinstance(params, list) and 3 <= len(params) <= 128, 'invalid parameter count')
    keys, labels, properties, roles = set(), set(), set(), []
    for i, p in enumerate(params):
        require(set(p) <= {'id','key','label','type','role','default','min','max','options','property','tolerance','visible'}, 'unknown parameter field')
        require(type(p['id']) is int and p['id'] == i, 'IDs must be contiguous; retain removed slots as reserved')
        require(re.fullmatch(r'[a-z][a-z0-9_]*', p['key']) and p['key'] not in keys, 'duplicate/invalid key')
        keys.add(p['key'])
        require(0 < len(p['label'].encode('ascii')) <= 16 and p['label'] not in labels, 'duplicate/invalid FFGL label')
        labels.add(p['label'])
        t, role = p['type'], p.get('role', 'binding')
        require(t in ('boolean','integer','number','choice','text'), 'unsupported parameter type')
        require(role in ('enabled','target','identity','reserved','binding'), 'unsupported role')
        require(type(p.get('visible', True)) is bool, 'visible must be boolean')
        roles.append(role)
        require(role != 'identity' or (t == 'text' and p['label'] == 'Instance Key'), 'identity contract changed')
        require(t != 'text' or role in ('identity','reserved'), 'text bindings unsupported')
        if t != 'text':
            require(number(p['default']), 'default must be finite')
            if t == 'boolean':
                require(p['default'] in (0,1), 'invalid boolean default')
            elif t == 'choice':
                opts = p['options']
                require(isinstance(opts,list) and 0 < len(opts) <= 256 and all(isinstance(v,str) and v for v in opts), 'invalid choices')
                require(len(set(opts)) == len(opts), 'duplicate choice')
                require(type(p['default']) is int and 0 <= p['default'] < len(opts), 'choice default out of range')
            else:
                require(number(p['min']) and number(p['max']) and p['min'] <= p['default'] <= p['max'] and p['min'] < p['max'], 'invalid numeric range')
                if t == 'integer':
                    require(all(float(p[k]).is_integer() for k in ('min','max','default')), 'integer range/default required')
        if role == 'binding':
            prop = p['property']
            require(re.fullmatch(r'[a-zA-Z_][a-zA-Z_0-9]*(\.[a-zA-Z_][a-zA-Z_0-9]*)*', prop) and prop not in properties, 'invalid/duplicate property')
            properties.add(prop)
        else:
            require('property' not in p, 'non-binding cannot set a property')
        require(number(p.get('tolerance',0)) and p.get('tolerance',0) >= 0, 'invalid tolerance')
        require(not p.get('tolerance',0) or t == 'number', 'tolerance only supported for numbers')
        if role == 'enabled': require(t == 'boolean' and p['default'] in (0, 1), 'Enabled must be boolean')
        if role == 'target': require(t == 'integer' and p['min'] >= 1 and p['max'] <= 1024, 'invalid target layer range')
    require(all(roles.count(r) == 1 for r in ('enabled','target','identity')), 'exactly one enabled, target and identity required')
    require(bool(properties), 'at least one binding required')
    for old in abi.get(d['pluginId'], []):
        require(old['id'] < len(params), 'deleted ABI slot; retain as reserved')
        new = params[old['id']]
        for field in ('key','type','options','min','max'):
            require(new.get(field) == old.get(field), f'ABI {field} changed at slot {old["id"]}')
        if new.get('role') != 'reserved':
            for field in ('role','property'):
                require(new.get(field) == old.get(field), f'ABI {field} changed')
    return d


def generate(definition, output, abi_path, check=False):
    d = validate(json.loads(Path(definition).read_text(encoding='utf-8')), json.loads(Path(abi_path).read_text(encoding='utf-8')))
    payload = json.dumps(d, ensure_ascii=True, separators=(',', ':'))
    # JSON-escaped ordinary C++ literal avoids raw-string terminator injection.
    literal = json.dumps(payload)
    content = '// Generated by tools/generate_plugin.py; edit definitions/*.json instead.\n#pragma once\n#include <nlohmann/json.hpp>\nnamespace definitions {\ninline const nlohmann::json& '+d['symbol']+'() {\n    static const auto value = nlohmann::json::parse('+literal+');\n    return value;\n}\n}\n'
    target = Path(output)
    if check:
        require(target.exists() and target.read_text(encoding='utf-8') == content, 'generated header is stale')
    elif not target.exists() or target.read_text(encoding='utf-8') != content:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content, encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--definition', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--abi', default=str(Path(__file__).resolve().parents[1]/'definitions/abi.json'))
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    generate(args.definition, args.output, args.abi, args.check)
