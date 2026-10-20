"""Parse gameplay expressions from user-owned SP/SUB files.

Generated from the port's verified parser logic. No converted game graph,
English dialogue, media, ROM, BIOS or pre-extracted input is included.
"""
from pathlib import Path
import hashlib, json, re

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def _parse(*, source_root, analysis_path, output_root=None, extend_factory=False, extend_factory_commands=False, extend_backup_common=False, factory_entries=(), next_module_entries=(), extend_headquarters=False, extend_practice_results=False, extend_jordan=False, extend_people=False, extend_inventory=False, extend_communicator=False, extend_transport=False, extend_gibson=False, extend_alton=False, extend_plato=False, extend_outer=False, extend_apartment=False, extend_ivan=False, extend_freddy=False, catalog_path=None, relative_children=False, extend_case_save=False, extend_act2=False, extend_act2_headquarters=False, extend_act2_practice=False, extend_act2_chief=False, extend_act2_transport=False, extend_act2_alton=False, extend_act2_contact=False, extend_act2_hospital=False, extend_act2_gibson=False, extend_act2_apartment=False, extend_act2_queens=False, extend_act2_outer=False, extend_act2_crash=False, extend_act2_maze=False, extend_act2_reconstruct=False, extend_act2_sp32=False, extend_act2_sp33=False, extend_act3_intro=False, extend_act3_palace=False, extend_act3_ending=False):
    SOURCE = Path(source_root).resolve(strict=True)
    SCRIPT = SOURCE / 'work/iso/SP06.BIN'
    SUB = SOURCE / 'work/iso/SUBCODE.BIN'
    ANALYSIS = Path(analysis_path).resolve(strict=True)
    output = Path(output_root).resolve()
    catalog = Path(catalog_path).resolve(strict=True)
    extra_factory = {2103, 3550} if extend_factory else set(factory_entries)
    assert not extend_factory_commands or (extend_factory and output_root is not None), 'Complete SP07 calls require a private factory import'
    assert extra_factory <= {2103, 3550}
    assert not extra_factory or output_root is not None, 'New story branches require private verification first'
    next_entries = set(next_module_entries)
    assert next_entries <= {16, 81, 433}
    assert not next_entries or (output_root is not None and 3550 in extra_factory), 'SP08 audit requires the original factory exit'
    assert not extend_headquarters or (output_root is not None and next_entries == {16, 81, 433}), 'Full HQ import requires the verified return entries'
    assert not extend_practice_results or extend_headquarters, 'SP19 requires the original SP08 caller'
    assert not extend_jordan or output_root is not None, 'Jordan import remains private until its UI/runtime is verified'
    assert not extend_people or extend_jordan, 'SP05 people lookup requires the original Jordan caller'
    assert not extend_inventory or output_root is not None, 'Inventory extension requires a private source/runtime audit'
    assert not extend_communicator or extend_headquarters, 'SP18 contact/transport menus require the original SP08 caller'
    assert not extend_transport or extend_communicator, 'SP17 requires the original HQ and phone callers'
    assert not extend_gibson or extend_transport, 'SP09 requires the original SP17 transport caller'
    assert not extend_alton or extend_gibson, 'SP10 requires the original Gibson/phone/transport progression'
    assert not extend_plato or extend_alton, 'SP11 requires the original Alton information progression'
    assert not extend_outer or extend_plato, 'SP12 requires the original transport and mask progression'
    assert not extend_apartment or (extend_transport and output_root is not None), 'SP03 requires the original SP17 caller'
    assert not extend_ivan or (extend_outer and extend_jordan and (output_root is not None)), 'SP14 requires original SP17/0018 and the montage progression'
    assert not extend_freddy or (extend_ivan and output_root is not None), 'SP15 requires the original SP17/0004 caller and Ivan progression'
    assert not extend_case_save or (extend_freddy and output_root is not None), 'SP16 requires the original SP15/0DE9 investigation completion caller'
    assert not extend_act2 or (extend_case_save and relative_children and (output_root is not None)), 'SP20 requires original SP16 completion and relative children'
    assert not extend_act2_headquarters or extend_act2, 'SP21 requires the original SP20 recap continuation'
    assert not extend_act2_practice or extend_act2_headquarters, 'SP20 practice evaluation is called from SP21'
    assert not extend_act2_chief or extend_act2_practice, 'SP22 follows the original SP21 headquarters'
    assert not extend_act2_transport or extend_act2_practice, 'SP34 requires the original ACT2 headquarters'
    assert not extend_act2_alton or extend_act2_transport, 'SP23 follows the original SP34 travel'
    assert not extend_act2_contact or extend_act2_alton, 'SP35 requires original ACT2 room callers'
    assert not extend_act2_hospital or extend_act2_contact, 'SP24 requires the original Napoleon appointment and hospital clues'
    assert not extend_act2_gibson or extend_act2_hospital, 'SP25 follows the original SP34 investigation call'
    assert not extend_act2_apartment or extend_act2_gibson, 'SP28 follows the original SP25 clues and SP34 apartment destination'
    assert not extend_act2_queens or extend_act2_apartment, 'SP26 requires the original SP28 hospital-name discovery'
    assert not extend_act2_outer or extend_act2_queens, 'SP27 follows the original Queens evidence and SP35 wolfman hint'
    assert not extend_act2_crash or extend_act2_outer, 'SP29 follows the original SP27 Benson answer and SP34 transport caller'
    assert not extend_act2_maze or extend_act2_crash, 'SP30 requires the original SP29 escape and SP26 hidden-stairs route'
    assert not extend_act2_reconstruct or extend_act2_maze, 'SP31 requires the original SP30 morgue investigation caller'
    assert not extend_act2_sp32 or extend_act2_reconstruct, 'SP32 requires the original SP31 reconstruction continuation'
    assert not extend_act2_sp33 or extend_act2_sp32, 'SP33 requires the original SP32 taxi continuation'
    assert not extend_act3_intro or extend_act2_sp33, 'SP36 requires the original SP33 chapter transfer'
    assert not extend_act3_palace or extend_act3_intro, 'SP37 requires the original SP36 transfer'
    assert not extend_act3_ending or extend_act3_palace, 'SP38 requires the original SP37 final encounter'
    if output_root is not None:
        assert output.is_relative_to(SOURCE.parent), 'Parser output must stay in its private workspace'
    b = SCRIPT.read_bytes()
    sub = SUB.read_bytes()
    module_id = 63
    file_id = 'SP06.BIN'
    assert sha(SCRIPT) == '47f2c8a5d6f5bff67b8246fd2ce1b7a143a06ace6547191992945efbd9c40714'
    assert sha(SUB) == '37d2bb27883acfedd6b05585fa5189c832bc6c94d37b3f5cf7c9a241142c88ac'
    module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == 'SP06.BIN'))
    entries = {int(e['text_offset'], 16): e for e in module['entries']}
    rows = {r['entry_id']: r for r in map(json.loads, catalog.read_text(encoding='utf-8').splitlines()) if r.get('record_type') == 'entry'}
    nodes = []
    memo = {}
    texts = {}
    active = set()
    targets = set()
    descriptors = {}

    def word(pc):
        return int.from_bytes(b[pc:pc + 2], 'big')

    def literal(pc, kind):
        i = len(nodes)
        nodes.append({'pc': pc, 'op': kind, 'arg': word(pc), 'arg2': 0, 'left': -1, 'right': -1, 'speaker': 0, 'end': pc + 2, 'source_hex': b[pc:pc + 2].hex()})
        return (i, pc + 2)

    def text_ref(offset, menu=False):
        entry = entries[offset]
        r = dict(rows[entry['entry_id']])
        assert r['source'] == entry['text'], entry['entry_id']
        source_key = not r['translation_ko'] and module_id in (62, 75, 66, 84, 85, 92) and menu and bool(re.fullmatch('[A-Z._ -]+', r['source']))
        if not r['translation_ko']:
            punctuation = re.sub('\\{raw:(?:F2|F6|F9|FA|FB|FC|F4|FE)\\}', '', r['source'])
            assert source_key or re.fullmatch('[\\d.\\s!?…()]+', punctuation), entry['entry_id']
            r['translation_ko'] = r['source']
        if module_id == 64 and offset in (1022, 4749):
            assert '{raw:EC}' in r['source']
            r['translation_ko'] = r['translation_ko'].replace('{raw:EC}', 'A')
        value = r['translation_ko'].replace('{raw:EC}', '[EC]').replace('{raw:EE}', '[EE]').replace('{raw:F0}', '[F0]').replace('{raw:F2}', '\n').replace('{raw:F6}', '\x0c')
        controls = 'F9|FA|FB|FC|F4|FE' if module_id in (61, 62) else 'F9|FA|FB|FC|F4'
        value = re.sub('\\{raw:(?:' + controls + ')\\}', '', value)
        assert '{raw:' not in value, (r['entry_id'], re.findall('\\{raw:[^}]+\\}', value))
        pages = []
        for authored in value.split('\x0c'):
            lines = []
            line = ''
            width = 0
            for ch in authored:
                advance = 6 if ch == ' ' else 12
                if ch == '\n':
                    lines.append(line)
                    line = ''
                    width = 0
                    continue
                if width + advance > 216:
                    lines.append(line)
                    line = ''
                    width = 0
                line += ch
                width += advance
            lines.append(line)
            pages.extend(('\n'.join(lines[i:i + 8]) for i in range(0, len(lines), 8)))
        assert ''.join((p.replace('\n', '') for p in pages)) == value.replace('\n', '').replace('\x0c', '')
        texts[module_id, offset] = {'module_id': module_id, 'entry_id': r['entry_id'], 'source_text_en': r['source'], 'translation_ko': r['translation_ko'], 'translation_status': r['status'], 'source_pages': value.split('\x0c'), 'pages': pages, 'original_offset': hex(offset), 'source_F4': '{raw:F4}' in r['source']}
        if source_key:
            texts[module_id, offset].update(source_search_key=True, catalog_translation_ko=None)
        return offset

    def parse(pc):
        assert 77 <= pc < (14336 if extend_inventory else 8116) if module_id == 63 else 0 <= pc < 14336, f'Out-of-scope expression {file_id}:{pc:#x}'
        if pc in memo:
            return memo[pc]
        assert pc not in active, f'Expression cycle {pc:#x}'
        active.add(pc)
        op = b[pc]
        i = len(nodes)
        n = {'pc': pc, 'op': op, 'arg': 0, 'arg2': 0, 'left': -1, 'right': -1, 'speaker': 0}
        nodes.append(n)
        desc = sub[39132 + op * 4:39136 + op * 4].hex()
        descriptors[hex(op)] = desc
        if op == 67:
            assert b[pc + 1:pc + 3] == b' \x00' and (2 <= b[pc + 3] < 49 or (module_id in (61, 62) and b[pc + 3] == 1))
            n['arg'] = text_ref(word(pc + 4))
            n['speaker'] = b[pc + 3]
            end = pc + 6
        elif op == 68:
            assert desc == '02001ec0'
            n['arg'] = text_ref(word(pc + 1))
            end = pc + 3
        elif op in (19, 20, 14):
            n['arg'] = word(pc + 1)
            end = pc + 3
        elif op == 12:
            n['arg'] = word(pc + 1)
            targets.add(n['arg'])
            end = pc + 3
        elif op == 43:
            n['arg'] = word(pc + 1)
            targets.add(n['arg'])
            end = pc + 3
        elif op in (45, 70, 72):
            end = pc + 1
        elif op == 58:
            assert desc == '07001dd2'
            end = pc + 1
        elif op in (54, 55, 57):
            assert desc == {54: '07401c2e', 55: '07701c2e', 57: '07001c78'}[op]
            if op == 54:
                n['left'], end = parse(pc + 1)
            else:
                end = pc + 1
        elif op == 73:
            assert desc == '07001f94'
            end = pc + 1
        elif op == 71:
            assert desc == '07002008'
            assert sub[40968:40984].hex() == '4eb900016c9066024e753afc00134e75'
            end = pc + 1
        elif op in (0, 39, 48, 66, 11, 46):
            n['left'], end = parse(pc + 1)
        elif op in (4, 10, 15, 13, 61, 69, 74, 53):
            n['arg'] = word(pc + 1)
            end = pc + 3
        elif op == 65:
            assert desc == '06002060'
            n['arg'] = word(pc + 1)
            end = pc + 3
        elif op == 21:
            n['arg'] = word(pc + 1)
            end = pc + 3
        elif op in (16, 17):
            if op == 17:
                assert desc == '043022c2'
            n['left'], at = parse(pc + 1)
            n['right'], end = literal(at, 257)
        elif op == 5:
            n['arg'] = word(pc + 1)
            end = pc + 3
        elif op == 44:
            n['left'], end = parse(pc + 1)
        elif op == 33:
            n['arg'] = word(pc + 1)
            n['arg2'] = word(pc + 3)
            targets.add(n['arg2']) if n['arg'] == 63 else None
            end = pc + 5
        elif op == 31:
            n['arg'] = word(pc + 1)
            n['left'], end = parse(pc + 3)
        elif op == 36:
            n['arg'] = word(pc + 1)
            n['left'], end = parse(pc + 3)
        elif op in (30, 34, 35):
            if op == 30:
                assert desc == '03301a12'
            n['arg'] = word(pc + 1)
            n['arg2'] = word(pc + 3)
            end = pc + 5
        elif op == 37:
            n['left'], at = parse(pc + 1)
            n['right'], end = parse(at)
        elif op == 38:
            n['left'], at = parse(pc + 3)
            assert at == word(pc + 1), f'Bad link {pc:#x} {at:#x}'
            n['right'], end = parse(at)
        elif op == 25:
            n['arg'] = word(pc + 3)
            n['left'], at = parse(pc + 5)
            assert at == word(pc + 1)
            n['right'], end = parse(at)
        elif op == 24:
            n['left'], at = parse(pc + 3)
            n['right'], at = parse(at)
            assert at == word(pc + 1)
            n['arg2'], end = parse(at)
        elif op == 26:
            n['left'], at = parse(pc + 1)
            n['right'], end = parse(at)
        elif op in (1, 3, 6, 7, 8, 9, 2, 22, 18, 23, 40, 41, 42, 49, 50, 51):
            if op in (1, 3):
                assert desc == {1: '0410224a', 3: '0110224a'}[op]
            if op == 49:
                assert desc == '0440228a'
            if op == 50:
                assert desc == '0410228a'
            if op == 51:
                assert desc == '0110228a'
            if op == 22:
                assert desc == '01302352'
            if op == 23:
                assert desc == '0130232e'
            if op == 42:
                assert desc == '013022e6'
            if op == 41:
                assert desc == '0130230a'
            if op == 40:
                assert desc == '0430230a'
            n['left'], at = parse(pc + 1) if op in (1, 6, 7, 40, 49, 50) else literal(pc + 1, 256)
            n['right'], end = literal(at, 257) if op in (2, 22, 18, 23, 40, 41, 42) else parse(at) if op in (6, 8, 49) else literal(at, 256)
        elif op in (59, 60):
            assert desc == {59: '05001e66', 60: '03001e66'}[op]
            n['arg'] = word(pc + 1)
            end = pc + 3
        elif op == 47:
            n['arg'] = word(pc + 1)
            end = pc + 3
        elif op == 28:
            assert desc == '01302376'
            n['arg'] = word(pc + 1)
            n['left'], end = literal(pc + 3, 257)
        elif op == 29:
            n['arg'] = word(pc + 1)
            n['left'], end = parse(pc + 3)
        elif op in (27, 52, 56, 62, 63, 64):
            if op == 64:
                assert desc == '01401a96'
            n['arg'] = word(pc + 1)
            if op == 56:
                text_ref(n['arg'], menu=True)
            n['left'], end = parse(pc + 3)
        else:
            raise ValueError(f'Unknown/unimplemented opcode {op:#x} at {file_id}:{pc:#x}, descriptor {desc}')
        n['end'] = end
        n['source_hex'] = b[pc:end].hex()
        memo[pc] = (i, end)
        active.remove(pc)
        return (i, end)
    parse(77)
    boundaries = {} if extend_inventory else {8116: 'inventory', 8254: 'use inventory'}
    while True:
        pending = targets - set(memo) - set(boundaries)
        if not pending:
            break
        for pc in sorted(pending):
            parse(pc)
    old_memo = memo
    old_boundaries = boundaries
    for n in nodes:
        n['module_id'] = module_id
    module_id = 64
    file_id = 'SP07.BIN'
    script7 = SOURCE / 'work/iso/SP07.BIN'
    b = script7.read_bytes()
    assert sha(script7) == '3332eb5e7602c16d4faabef2e03286ee7be8464aca7f97dfed6a75651d6fdb64'
    module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
    entries = {int(e['text_offset'], 16): e for e in module['entries']}
    start7 = len(nodes)
    memo = {}
    targets = set()
    active = set()
    parse(4)
    boundaries = {}
    while True:
        pending = targets - set(memo) - set(boundaries)
        if not pending:
            break
        for pc in sorted(pending):
            if extend_factory_commands or (pc < 5774 and (pc in extra_factory or pc not in (3550, 2103))) or pc == 6238:
                parse(pc)
            else:
                boundaries[pc] = 'Later first-case command'
    for n in nodes[start7:]:
        n['module_id'] = module_id
    all_memo = {(63, pc): i for pc, (i, end) in old_memo.items()} | {(module_id, pc): i for pc, (i, end) in memo.items()}
    extra_sources = []
    next_boundaries = {}
    if next_entries:
        module_id = 65
        file_id = 'SP08.BIN'
        script8 = SOURCE / 'work/iso/SP08.BIN'
        b = script8.read_bytes()
        assert sha(script8) == '54024e3e0f93d46e84dbc2b752c62ce47afb0cc8c9b3d2b05069ccd8101514c1'
        extra_sources.append(script8)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start8 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(16)
        while True:
            pending = targets - set(memo) - set(next_boundaries)
            if not pending:
                break
            for pc in sorted(pending):
                if pc in next_entries or extend_headquarters:
                    parse(pc)
                else:
                    next_boundaries[pc] = 'Later SP08 command; runtime not enabled'
        for n in nodes[start8:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_practice_results:
        module_id = 76
        file_id = 'SP19.BIN'
        script19 = SOURCE / 'work/iso/SP19.BIN'
        b = script19.read_bytes()
        extra_sources.append(script19)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start19 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start19:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_jordan:
        module_id = 61
        file_id = 'SP04.BIN'
        script4 = SOURCE / 'work/iso/SP04.BIN'
        b = script4.read_bytes()
        extra_sources.append(script4)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start4 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start4:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_people:
        module_id = 62
        file_id = 'SP05.BIN'
        script5 = SOURCE / 'work/iso/SP05.BIN'
        b = script5.read_bytes()
        extra_sources.append(script5)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start5 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start5:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_communicator:
        module_id = 75
        file_id = 'SP18.BIN'
        script18 = SOURCE / 'work/iso/SP18.BIN'
        b = script18.read_bytes()
        extra_sources.append(script18)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start18 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start18:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
        contact_entries = {nodes[n['left']]['arg2'] for n in nodes[start18:] if n['op'] == 11 and nodes[n['left']]['op'] == 33 and (nodes[n['left']]['arg'] == 59)}
        module_id = 59
        file_id = 'SP02.BIN'
        script2 = SOURCE / 'work/iso/SP02.BIN'
        b = script2.read_bytes()
        extra_sources.append(script2)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start2 = len(nodes)
        memo = {}
        targets = set(contact_entries)
        active = set()
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start2:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_transport:
        module_id = 74
        file_id = 'SP17.BIN'
        script17 = SOURCE / 'work/iso/SP17.BIN'
        b = script17.read_bytes()
        extra_sources.append(script17)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start17 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start17:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_gibson:
        module_id = 66
        file_id = 'SP09.BIN'
        script9 = SOURCE / 'work/iso/SP09.BIN'
        b = script9.read_bytes()
        extra_sources.append(script9)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start9 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start9:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_alton:
        module_id = 67
        file_id = 'SP10.BIN'
        script10 = SOURCE / 'work/iso/SP10.BIN'
        b = script10.read_bytes()
        extra_sources.append(script10)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start10 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start10:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_plato:
        module_id = 68
        file_id = 'SP11.BIN'
        script11 = SOURCE / 'work/iso/SP11.BIN'
        b = script11.read_bytes()
        extra_sources.append(script11)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start11 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start11:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_outer:
        module_id = 69
        file_id = 'SP12.BIN'
        script12 = SOURCE / 'work/iso/SP12.BIN'
        b = script12.read_bytes()
        extra_sources.append(script12)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start12 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(12)
        parse(2191)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start12:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
        module_id = 70
        file_id = 'SP13.BIN'
        script13 = SOURCE / 'work/iso/SP13.BIN'
        b = script13.read_bytes()
        extra_sources.append(script13)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start13 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start13:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_apartment:
        module_id = 60
        file_id = 'SP03.BIN'
        script3 = SOURCE / 'work/iso/SP03.BIN'
        b = script3.read_bytes()
        assert sha(script3) == '2370f3bd354c2d75b7158f8d41f3f3b61db72a9daa4709fc1011468f08c0e9d4'
        extra_sources.append(script3)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start3 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start3:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_ivan:
        module_id = 71
        file_id = 'SP14.BIN'
        script14 = SOURCE / 'work/iso/SP14.BIN'
        b = script14.read_bytes()
        assert sha(script14) == '4a16d43745c0695ed999fa90cf473d8f6059494d434de8bb7018f6c09b63c80d'
        extra_sources.append(script14)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start14 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(24)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start14:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_freddy:
        module_id = 72
        file_id = 'SP15.BIN'
        script15 = SOURCE / 'work/iso/SP15.BIN'
        b = script15.read_bytes()
        assert sha(script15) == '329f57d4dc850a5d113fb3b2f22126f57046f09913cb4ad136f0d5ee3748ad9c'
        extra_sources.append(script15)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start15 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start15:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_case_save:
        module_id = 73
        file_id = 'SP16.BIN'
        script16 = SOURCE / 'work/iso/SP16.BIN'
        b = script16.read_bytes()
        assert sha(script16) == '91b41b6db9689a7d26185ec0b0086c13f47402eee2b1510c94b131aef835fab3'
        extra_sources.append(script16)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start16 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start16:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2:
        module_id = 77
        file_id = 'SP20.BIN'
        script20 = SOURCE / 'work/iso/SP20.BIN'
        b = script20.read_bytes()
        assert sha(script20) == 'f7ff16a19169f33851761a00e96ff520c430ec41fede16cca1cecc2795f387da'
        extra_sources.append(script20)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start20 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start20:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_headquarters:
        module_id = 78
        file_id = 'SP21.BIN'
        script21 = SOURCE / 'work/iso/SP21.BIN'
        b = script21.read_bytes()
        assert sha(script21) == '0d7f934d497ac15f8a3fced41e545506c224af94ca3d2d559691575782ad5759'
        extra_sources.append(script21)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start21 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        targets.add(88)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start21:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_practice:
        module_id = 77
        file_id = 'SP20.BIN'
        b = script20.read_bytes()
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start_practice = len(nodes)
        memo = {pc: (i, nodes[i]['end']) for (mid, pc), i in all_memo.items() if mid == module_id}
        targets = set()
        active = set()
        parse(43)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start_practice:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_chief:
        module_id = 79
        file_id = 'SP22.BIN'
        script22 = SOURCE / 'work/iso/SP22.BIN'
        b = script22.read_bytes()
        assert sha(script22) == '74cfead189a115dae226c64ef0520a8b2ab404189771c7e0de4bd86b6cbc2ede'
        extra_sources.append(script22)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start22 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start22:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_transport:
        module_id = 91
        file_id = 'SP34.BIN'
        script34 = SOURCE / 'work/iso/SP34.BIN'
        b = script34.read_bytes()
        assert sha(script34) == '5f689115244859df9c88220704e846108a37edcce1715d90c7a057e08f9c516e'
        extra_sources.append(script34)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start34 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start34:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_alton:
        module_id = 80
        file_id = 'SP23.BIN'
        script23 = SOURCE / 'work/iso/SP23.BIN'
        b = script23.read_bytes()
        assert sha(script23) == '038e9137e24d7ef9a3682cde07544269afeac934b9ba70d01806a7093bc5efc8'
        extra_sources.append(script23)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start23 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        parse(8)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start23:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_contact:
        module_id = 92
        file_id = 'SP35.BIN'
        script35 = SOURCE / 'work/iso/SP35.BIN'
        b = script35.read_bytes()
        assert sha(script35) == '90e212f5c9d8cecc74c63c2330e28a8700d7f9e5f27a937c34dc41b0d68682e1'
        extra_sources.append(script35)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start35 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start35:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_hospital:
        module_id = 81
        file_id = 'SP24.BIN'
        script24 = SOURCE / 'work/iso/SP24.BIN'
        b = script24.read_bytes()
        assert sha(script24) == '672d5300607e80f84563710203242a35b6a77ebfce323d123a52b0fc9fbe4e1d'
        extra_sources.append(script24)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start24 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start24:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_gibson:
        module_id = 82
        file_id = 'SP25.BIN'
        script25 = SOURCE / 'work/iso/SP25.BIN'
        b = script25.read_bytes()
        assert sha(script25) == 'f7dd6ddf96261f79e1568560f577ca76de44373796f1436600c06685e0ccdc7a'
        extra_sources.append(script25)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start25 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start25:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_apartment:
        module_id = 85
        file_id = 'SP28.BIN'
        script28 = SOURCE / 'work/iso/SP28.BIN'
        b = script28.read_bytes()
        assert sha(script28) == '9ec6a79eecdc54fa603563f87c24d47d5eb04b36d6ca6761d776142a337afd5f'
        extra_sources.append(script28)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start28 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start28:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_queens:
        module_id = 83
        file_id = 'SP26.BIN'
        script26 = SOURCE / 'work/iso/SP26.BIN'
        b = script26.read_bytes()
        assert sha(script26) == '368632e21af599b4a8e4bac91b35d7453da0e29e46fee20f14ed4c7b15c5e4d3'
        extra_sources.append(script26)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start26 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        if extend_act2_crash:
            parse(8)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start26:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_outer:
        module_id = 84
        file_id = 'SP27.BIN'
        script27 = SOURCE / 'work/iso/SP27.BIN'
        b = script27.read_bytes()
        assert sha(script27) == '19cdbdfae6f626a2e54cf31428634353d29e23282c7697a18978103ddbde8b59'
        extra_sources.append(script27)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start27 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start27:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_crash:
        module_id = 86
        file_id = 'SP29.BIN'
        script29 = SOURCE / 'work/iso/SP29.BIN'
        b = script29.read_bytes()
        assert sha(script29) == 'e814bc4a84fd8aa09c5045a9cc5901b1b60d350ea1bec71548763585cee8a41f'
        extra_sources.append(script29)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start29 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start29:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_maze:
        module_id = 87
        file_id = 'SP30.BIN'
        script30 = SOURCE / 'work/iso/SP30.BIN'
        b = script30.read_bytes()
        assert sha(script30) == 'c3f43e8bf71a5d9de915573d01c5a311ad5c5219b2fea975232ea6d1ba90c05e'
        extra_sources.append(script30)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start30 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        parse(32)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start30:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_reconstruct:
        module_id = 88
        file_id = 'SP31.BIN'
        script31 = SOURCE / 'work/iso/SP31.BIN'
        b = script31.read_bytes()
        assert sha(script31) == 'a5b8cbe2bdd8308710f204d1c7dbd993ad6f80f40ed0739289409c5327096d09'
        extra_sources.append(script31)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start31 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start31:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_sp32:
        module_id = 89
        file_id = 'SP32.BIN'
        script32 = SOURCE / 'work/iso/SP32.BIN'
        b = script32.read_bytes()
        assert sha(script32) == '18e743a61fc8852cf1f65ded267728ef83fbb53e358f9b4ee5455109de860361'
        extra_sources.append(script32)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start32 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        parse(8)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start32:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act2_sp33:
        module_id = 90
        file_id = 'SP33.BIN'
        script33 = SOURCE / 'work/iso/SP33.BIN'
        b = script33.read_bytes()
        assert sha(script33) == 'f23738636c1f238b9180b140605dc54e8c9168412fa261db08d8cb9b878fd8c0'
        extra_sources.append(script33)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start33 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start33:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act3_intro:
        module_id = 93
        file_id = 'SP36.BIN'
        script36 = SOURCE / 'work/iso/SP36.BIN'
        b = script36.read_bytes()
        assert sha(script36) == '2861a27bf92cccd30c3adab7c11d9398804d6ddf204c577184a948be1df6f99f'
        extra_sources.append(script36)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start36 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start36:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act3_palace:
        module_id = 94
        file_id = 'SP37.BIN'
        script37 = SOURCE / 'work/iso/SP37.BIN'
        b = script37.read_bytes()
        assert sha(script37) == 'f4d76833c9d9e2a0198cee1f384f046e82fca86a20441bad84b139347e088a3c'
        extra_sources.append(script37)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start37 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start37:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_act3_ending:
        module_id = 95
        file_id = 'SP38.BIN'
        script38 = SOURCE / 'work/iso/SP38.BIN'
        b = script38.read_bytes()
        assert sha(script38) == '1aa24522179b6585b938ce73fc8314faa7ad7500f648e4e30b5e14557ee4f0b0'
        extra_sources.append(script38)
        module = next((m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id))
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        start38 = len(nodes)
        memo = {}
        targets = set()
        active = set()
        parse(0)
        parse(4)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[start38:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if extend_backup_common:
        assert extend_factory_commands and output_root is not None, 'Common backup needs the complete private factory graph'
        module_id = 0x39
        file_id = 'SP00.BIN'
        common_script = SOURCE / 'work/iso/SP00.BIN'
        b = common_script.read_bytes()
        extra_sources.append(common_script)
        module = next(m for m in json.loads(ANALYSIS.read_text(encoding='utf-8'))['files'] if m['file_id'] == file_id)
        entries = {int(e['text_offset'], 16): e for e in module['entries']}
        common_start = len(nodes)
        memo, targets, active = {}, set(), set()
        parse(7)
        while targets - set(memo):
            for pc in sorted(targets - set(memo)):
                parse(pc)
        for n in nodes[common_start:]:
            n['module_id'] = module_id
        all_memo.update({(module_id, pc): i for pc, (i, end) in memo.items()})
    if relative_children:
        assert output_root is not None and len(nodes) < 2147483647
        for i, n in enumerate(nodes):
            for key in ('left', 'right'):
                target = n[key]
                assert target == -1 or (0 < target - i <= 32767 and nodes[target]['module_id'] == n['module_id']), (i, key, target)
            if n['op'] == 24:
                assert 0 < n['arg2'] - i <= 65535 and nodes[n['arg2']]['module_id'] == n['module_id']
    else:
        assert len(nodes) <= 32767, f'Story graph {len(nodes)} exceeds the current signed child-index ABI'

    def menu_shape(i):
        n = nodes[i]
        if 'menu_shape' in n:
            return n['menu_shape']
        op = n['op']
        value = op in (56, 45)
        if op in (37, 38):
            value = menu_shape(n['left']) and menu_shape(n['right'])
        elif op in (39, 48, 27):
            value = menu_shape(n['left'])
        elif op == 26:
            value = menu_shape(n['right'])
        n['menu_shape'] = int(value)
        return value
    for i in range(len(nodes)):
        menu_shape(i)
    offsets = sorted(texts)
    ids = {o: i for i, o in enumerate(offsets)}
    for n in nodes:
        if n['op'] in (56, 67, 68):
            n['arg'] = ids[n['module_id'], n['arg']]
    result = {'kind': 'source_sp06_headquarters_expression_graph', 'start_node': old_memo[77][0], 'nodes': nodes, 'texts': [texts[o] for o in offsets], 'pc_to_node': {f'{mod}:{pc}': i for (mod, pc), i in all_memo.items()}, 'scene_boundaries': {f'63:{hex(pc)}': label for pc, label in old_boundaries.items()} | {f'64:{hex(pc)}': label for pc, label in boundaries.items()}, 'descriptors': descriptors, 'sources': [{'path': str(p), 'sha256': sha(p)} for p in (SCRIPT, script7, SUB, catalog, ANALYSIS)], 'limits': ['Static parsing does not certify runtime support for every branch', 'Bounded first-case scene renderer and stage1 combat; later modules remain unavailable', 'Controller codes EC/F0 retained as explicit tokens; affected actions unavailable', 'External modules and inventory LOOK/USE remain boundaries', 'Original PRNG stream not reproduced']}
    return result
