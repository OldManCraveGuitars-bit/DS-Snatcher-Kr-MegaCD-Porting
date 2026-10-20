"""Derive gameplay control arrays from user inputs, with no shipped game graph."""
from pathlib import Path
import hashlib,inspect,json,re,struct
from source_story_text import build as text_index
from source_story_parser import _parse


SCRIPT_NAMES=('SP00.BIN',)+tuple('SP%02d.BIN'%i for i in range(2,39))


def build(source,replacements,workspace):
    """Use a fresh caller-owned workspace; original input objects are read-only."""
    workspace=Path(workspace).resolve()
    workspace.mkdir(exist_ok=False,parents=True)
    original=text_index(source,SCRIPT_NAMES)
    entries={r['entry_id']:r['text'] for f in original['files'] for r in f['entries']}
    rows=[];seen=set()
    for row in replacements:
        if set(row)!={'record_type','entry_id','translation_ko','status'} or row['record_type']!='entry':
            raise ValueError('Expected replacement-only translation entry')
        ident=row['entry_id']
        if ident not in entries or ident in seen:raise ValueError('Unknown or duplicate translation ID: '+ident)
        # The authored catalog marks numeric/punctuation and untranslated search
        # keys with null. The parser validates every actually referenced null
        # against original key/punctuation rules; it cannot replace dialogue.
        if not (row['translation_ko'] is None or isinstance(row['translation_ko'],str)) or not isinstance(row['status'],str):
            raise ValueError('Invalid translation entry: '+ident)
        seen.add(ident);rows.append(dict(row,source=entries[ident]))
    if seen!=set(entries):raise ValueError('Translation is incomplete for the supplied disc')
    owned=workspace/'input';iso=owned/'work/iso';iso.mkdir(parents=True)
    for name in (*SCRIPT_NAMES,'SUBCODE.BIN'):(iso/name).write_bytes(source.read_file(name))
    analysis=workspace/'text-index.json'
    analysis.write_text(json.dumps(original,ensure_ascii=False),encoding='utf8')
    catalog=workspace/'runtime-catalog.jsonl'
    catalog.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf8')
    options={n:True for n in inspect.signature(_parse).parameters if n.startswith('extend_')}
    graph=_parse(source_root=owned,analysis_path=analysis,catalog_path=catalog,
        output_root=workspace/'parsed',next_module_entries=(0x10,0x51,0x1b1),relative_children=True,**options)
    apply_platform_instructions(graph)
    graph.update(kind='user_original_full_story_control_graph',
        sources=[dict(path=str(iso/f['file_id']),sha256=f['source_sha256']) for f in original['files']]+
            [dict(path=str(iso/'SUBCODE.BIN'),sha256=hashlib.sha256((iso/'SUBCODE.BIN').read_bytes()).hexdigest())],
        limits=['Static control conversion is separate from runtime and audio/visual verification.'])
    return graph


def apply_platform_instructions(graph):
    """Reapply the approved DS controls, retaining the full authored source text."""
    def wrap(value):
        pages=[]
        for authored in value.split('\f'):
            lines=[];line='';width=0
            for ch in authored:
                advance=6 if ch==' ' else 12
                if ch=='\n':lines.append(line);line='';width=0;continue
                if width+advance>216:lines.append(line);line='';width=0
                line+=ch;width+=advance
            lines.append(line);pages.extend('\n'.join(lines[i:i+8]) for i in range(0,len(lines),8))
        return pages
    blaster={'SP06.BIN/%04d'%n for n in (348,408,422,423,431,432,433)};seen=set()
    for row in graph['texts']:
        if row['entry_id'] in blaster:
            value=row['translation_ko'].replace('{raw:EC}','X').replace('{raw:F0}','A')
            value=value.replace('{raw:F2}','\n').replace('{raw:F6}','\f')
            value=re.sub(r'\{raw:(?:F9|FA|FB|FC|F4)\}','',value)
            if '{raw:' in value:raise ValueError('Unsupported blaster instruction control')
            row.update(pages=wrap(value),control_glyph_adaptation={'EC':'X','F0':'A'});seen.add(row['entry_id'])
        elif row['entry_id']=='SP26.BIN/0302':
            original_pages=row['pages']
            if '[EE]' not in original_pages[0]:raise ValueError('Unexpected original flashlight instruction')
            row['pages']=['손전등 버튼을 누른 뒤,\n아래 영역을 터치하거나 끌면\n조명이 움직입니다.\n명령 선택을 누르면\n평소처럼 명령을 고릅니다.']
            row['control_glyph_adaptation']=dict(platform='Nintendo DS',kind='lower_touch_flashlight',original_pages=original_pages,
                explanation='Source button+direction aiming is adapted to a lower touch pad; command-selection return remains explicit. Original target validation and clauses are preserved.')
            seen.add(row['entry_id'])
    if seen!=blaster|{'SP26.BIN/0302'}:raise ValueError('Missing platform instruction entry')


def control_arrays(graph):
    """Pack the original-derived eight-byte ARM9 records and sparse operands."""
    nodes=graph['nodes'];packed=bytearray();ids=[];values=[]
    if not 0<len(nodes)<65536:raise ValueError('Unsupported gameplay node count')
    for i,n in enumerate(nodes):
        right=-1 if n['right']<0 else n['right']-i
        if not (right==-1 or 0<right<512):raise ValueError('Gameplay right-child distance')
        if not (0<=n['pc']<16384 and 0<=n['op']<512 and 0<=n['module_id']<128 and 0<=n['speaker']<64
                and n['menu_shape'] in (0,1) and n['left'] in (-1,i+1) and 0<=n['arg']<=65535):
            raise ValueError('Unsupported gameplay record')
        packed+=struct.pack('<4H',n['pc']|(n['menu_shape']<<14)|(int(n['left']>=0)<<15),
            n['op']|(n['module_id']<<9),n['arg'],(right&1023)|(n['speaker']<<10))
        operand=n['arg2']-i if n['op']==0x18 else n['arg2']
        if not 0<=operand<=65535:raise ValueError('Gameplay sparse operand')
        if operand:ids.append(i);values.append(operand)
    texts=graph['texts']
    supported=[int(bool(t.get('control_glyph_adaptation')) or all(token not in t['translation_ko']
        for token in ('{raw:EC}','{raw:EE}','{raw:F0}'))) for t in texts]
    return dict(story_nodes=bytes(packed),story_arg2_nodes=struct.pack('<'+'I'*len(ids),*ids),
        story_arg2_values=struct.pack('<'+'H'*len(values),*values),
        story_text_offsets=struct.pack('<'+'H'*len(texts),*(int(t['original_offset'],16) for t in texts)),
        story_text_modules=struct.pack('<'+'H'*len(texts),*(t['module_id'] for t in texts)),
        story_text_supported=bytes(supported))


def search_entries(graph):
    keys=sorted({n['arg'] for n in graph['nodes'] if n['module_id'] in (62,75,66,0x5c,0x55,0x54) and n['op']==0x38})
    return [(key,graph['texts'][key]['source_text_en'].encode('ascii')+b'\0') for key in keys]


def source_headers(graph):
    """Recreate the player's graph and search headers only after disc conversion."""
    from keyboard_titles import source_header as keyboard_title_header
    arrays=control_arrays(graph);nodes=graph['nodes'];count=len(arrays['story_arg2_values'])//2
    def unpack(name,code):
        data=arrays[name];return struct.unpack('<'+code*(len(data)//struct.calcsize(code)),data)
    def declaration(ctype,name,code,space=False):
        return 'static const '+ctype+' '+name+'[]'+(' = ' if space else '=')+'{'+','.join(map(str,unpack(name,code)))+'};'
    header=['#pragma once','#include <stdint.h>','#define STORY_CHILD_RELATIVE 1',
        'typedef struct {uint16_t pc:14,menu_shape:1,has_left:1;uint16_t op:9,module_id:7;uint16_t arg;int16_t right:10;uint16_t speaker:6;} StoryNode;',
        'static const StoryNode story_nodes[] = {']
    for i,n in enumerate(nodes):
        row=(n['pc'],n['menu_shape'],int(n['left']>=0),n['op'],n['module_id'],n['arg'],
            -1 if n['right']<0 else n['right']-i,n['speaker'])
        header.append('{'+','.join(map(str,row))+'},')
    header+=['};','#define STORY_ARG2_COUNT '+str(count),declaration('uint32_t','story_arg2_nodes','I'),
        declaration('uint16_t','story_arg2_values','H'),'']
    for ctype,name,code in (('uint16_t','story_text_offsets','H'),('uint8_t','story_text_supported','B'),('uint16_t','story_text_modules','H')):
        header.append(declaration(ctype,name,code,True))
    header+=['#define STORY_START_NODE '+str(graph['start_node']),'#define STORY_NODE_COUNT '+str(len(nodes)),
        '#define STORY_TEXT_COUNT '+str(len(graph['texts']))]
    keys=[key for key,_ in search_entries(graph)]
    search=['#pragma once','typedef struct {unsigned text;const char *key;} StorySearchKey;',
        'static const StorySearchKey story_search_keys[] = {']
    search+=['{'+str(key)+','+json.dumps(graph['texts'][key]['source_text_en'])+'},' for key in keys]
    search+=['};','#define STORY_SEARCH_KEY_COUNT '+str(len(keys))]
    return {'story_program.h':'\n'.join(header),'story_search.h':'\n'.join(search),
            'keyboard_titles.h':keyboard_title_header(graph)}
