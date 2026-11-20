"""Export presentation metadata from an installed TF2 (no account access)."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re


def read_kv(path):
    raw = Path(path).read_bytes()
    text = raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')
    pattern = re.compile(r'\s+|//[^\n]*|"((?:\\.|[^"\\])*)"|([{}])|([^\s{}"]+)')
    tokens = []
    for m in pattern.finditer(text):
        if m[1] is not None:
            tokens.append(re.sub(r'\\(["\\nt])', lambda x: {'n':'\n','t':'\t'}.get(x[1],x[1]), m[1]))
        elif m[2] or m[3]:
            tokens.append(m[2] or m[3])
    it = iter(tokens)
    def block(nested=False):
        result = {}
        for key in it:
            if key == '}':
                if not nested: raise ValueError('Unexpected closing brace')
                return result
            value = next(it)
            result[key] = block(True) if value == '{' else value
        if nested: raise ValueError('Unclosed block')
        return result
    return block()


def merge(base, override):
    result = copy.deepcopy(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = merge(result[key], value)
        else:
            result[key] = copy.deepcopy(value)
    return result


def extract(tf):
    game = read_kv(tf/'scripts/items/items_game.txt')['items_game']
    tokens = {}
    for filename in ['tf_english.txt', 'tf_proto_obj_defs_english.txt']:
        tokens.update({k.lower(): v for k,v in read_kv(tf/'resource'/filename)['lang']['Tokens'].items()})
    def loc(token):
        return tokens.get(token.lstrip('#').lower(), '')
    prefabs = game['prefabs']
    cache = {}
    def resolve(value, trail=()):
        result = {}
        # Valve gives the first prefab priority over subsequent prefabs.
        for name in reversed(value.get('prefab', '').split()):
            if name in trail: raise ValueError('Cyclic prefab: '+name)
            if name not in cache: cache[name] = resolve(prefabs.get(name, {}), trail+(name,))
            result = merge(result, cache[name])
        return merge(result, value)
    items = {k:resolve(v) for k,v in game['items'].items() if k.isdigit()}
    return game, tokens, items, loc


def build_metadata(tf):
    game,tokens,items,loc = extract(tf)
    grades = {k:int(v['value']) for k,v in game['rarities'].items()}
    item_grades = {}
    for collection in game.get('item_collections', {}).values():
        if collection.get('is_reference_collection') == '1': continue
        for grade, entries in collection.get('items', {}).items():
            if grade in grades and isinstance(entries, dict):
                for name in entries: item_grades[name] = grades[grade]
    paints = {}
    for key,value in tokens.items():
        m = re.fullmatch(r'9_(\d+)_field \{ field_number: 2 \}',key)
        if m: paints[m[1]] = {'name':value,'grade':0}
    relevant = {'paintkit_proto_def_index', 'set_item_texture_wear', 'texture_wear_default',
                'attach particle effect', 'taunt attach particle index', 'elevate quality',
                'kill eater', 'kill eater 2', 'kill eater user 1', 'kill eater user 2', 'kill eater user 3',
                'tool target item', 'killstreak tier', 'killstreak effect', 'killstreak idleeffect', 'set item tint RGB', 'set item tint RGB 2',
                'always tradable', 'cannot trade', 'never craftable', 'cannot giftwrap', 'non economy', 'is marketable', 'limited quantity item'}
    attr_ids = {v['name']:k for k,v in game['attributes'].items() if v.get('name') in relevant}
    definitions = {}
    for index,value in items.items():
        attrs = dict(value.get('static_attrs',{}))
        attrs.update({k:v.get('value') for k,v in value.get('attributes',{}).items() if isinstance(v,dict)})
        stat_clock_capable = bool(attrs.get('weapon_uses_stattrak_module'))
        attrs = {attr_ids[k]:float(v) for k,v in attrs.items() if k in attr_ids and v is not None}
        grade = item_grades.get(value.get('name'), grades.get(value.get('item_rarity'),0))
        tool = value.get('tool',{}).get('type') == 'paintkit'
        decorated = value.get('item_quality') == 'paintkitweapon'
        name = (loc(value.get('base_item_name','')) or loc(value.get('item_name',''))) if decorated else ''
        if tool: name = loc('#TF_PaintKitTool')
        style=value.get('visuals',{}).get('styles',{}).get('1',{})
        gold_image=style.get('image_inventory','') if isinstance(style,dict) else ''
        if not gold_image.lower().endswith('_gold'):gold_image=''
        definitions[index] = {'australium_image':gold_image,
                              'can_craft_if_purchased':value.get('capabilities',{}).get('can_craft_if_purchased')=='1',
                              'can_gift_wrap':value.get('capabilities',{}).get('can_gift_wrap')=='1',
                              'decodable':value.get('capabilities',{}).get('decodable')=='1',
                              'definition_name':value.get('name',''), 'slot':value.get('item_slot',''),
                              'is_tool':bool(value.get('tool')), 'name':name,'grade':grade,'paint_tool':tool,'decorated':decorated,
                              'display_name':loc(value.get('base_item_name','')) or loc(value.get('item_name','')) or value.get('name',''),
                              'tool_type':value.get('tool',{}).get('type',''),
                              'stat_clock_capable':stat_clock_capable,
                              'stat_clock_scale':float(value.get('static_attrs',{}).get('weapon_stattrak_module_scale',1)),
                              'halloween_restricted':str(value.get('holiday_restriction','')).startswith('halloween'),
                              'pyrovision_restricted':bool(int(value.get('vision_filter_flags','0')) & 1),
                              'attributes':attrs}
        paint = str(int(attrs.get(attr_ids['paintkit_proto_def_index'],-1)))
        if tool and paint in paints and grade:
            if paints[paint]['grade'] not in (0,grade): raise ValueError('Conflicting paint grades: '+paint)
            paints[paint]['grade'] = grade
    cosmetic_paints={}
    for value in items.values():
        if value.get('tool',{}).get('type')!='paint_can':continue
        raw=value.get('attributes',{}).get('set item tint RGB',{}).get('value')
        name=loc(value.get('item_name',''))
        if raw and name:cosmetic_paints[str(int(float(raw)))]={'name':name,'colour':int(float(raw))}
    cosmetic_paints['1']={'name':'Team Spirit','colour':0xB8383B}
    effects = {k[len('attrib_particle'):]:v for k,v in tokens.items() if re.fullmatch(r'attrib_particle\d+',k)}
    source_files = ['scripts/items/items_game.txt','resource/tf_english.txt','resource/tf_proto_obj_defs_english.txt']
    return {'format':1,'sources':{p:hashlib.sha256((tf/p).read_bytes()).hexdigest() for p in source_files},
            'items':definitions,'paints':paints,'effects':effects,'cosmetic_paints':cosmetic_paints,
            'paint_spells':{key:loc(token) for key,token in game['string_lookups']['SPELL: set item tint RGB'].items()},
            'footprint_spells':{key:loc(token) for key,token in game['string_lookups']['SPELL: set Halloween footstep type'].items()},
            'sheens':{str(i):tokens.get('attrib_killstreakidleeffect'+str(i),'') for i in range(1,8)},
            'killstreakers':{str(i):tokens.get('attrib_killstreakeffect'+str(i),'') for i in range(2002,2009)}}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('tf', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    metadata = build_metadata(args.tf)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(metadata,ensure_ascii=False,indent=2),encoding='utf-8')
    print('Exported',len(metadata['items']),'items,',len(metadata['paints']),'finishes and',len(metadata['effects']),'effects')
