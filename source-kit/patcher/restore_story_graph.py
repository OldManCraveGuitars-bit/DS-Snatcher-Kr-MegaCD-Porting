"""Restore erased ARM9 gameplay tables from a newly converted user-owned disc."""
from pathlib import Path
import hashlib,json,tempfile
from derive_story_graph import build,control_arrays


ARRAY_NAMES=frozenset(('story_nodes','story_arg2_nodes','story_arg2_values',
    'story_text_offsets','story_text_modules','story_text_supported'))


def restore_tables(payload,records,arrays):
    if set(arrays)!=ARRAY_NAMES or not isinstance(records,list) or len(records)!=len(ARRAY_NAMES):
        raise ValueError('Incomplete derived gameplay tables')
    spans=[];names=set()
    for row in records:
        name=row['name'];start=row['offset'];size=row['size']
        if name not in ARRAY_NAMES or name in names or type(start) is not int or type(size) is not int:
            raise ValueError('Invalid gameplay table binding')
        if start<512 or size<=0 or start+size>len(payload) or any(payload[start:start+size]):
            raise ValueError('Gameplay table template must contain only erased bytes')
        if any(start<end and begin<start+size for begin,end in spans):
            raise ValueError('Overlapping gameplay table bindings')
        data=arrays[name]
        if len(data)!=size or hashlib.sha256(data).hexdigest()!=row['sha256']:
            raise ValueError('Unsupported derived gameplay table: '+name)
        names.add(name);spans.append((start,start+size))
    result=bytearray(payload)
    for row in records:result[row['offset']:row['offset']+row['size']]=arrays[row['name']]
    return bytes(result)


def restore_from_source(payload,description,source,engine,temporary_parent):
    filename=description['replacement_file']
    if not isinstance(filename,str) or Path(filename).name!=filename or ':' in filename or '\\' in filename:
        raise ValueError('Invalid replacement catalog filename')
    replacements=(Path(engine)/filename).read_bytes()
    if hashlib.sha256(replacements).hexdigest()!=description['replacement_sha256']:
        raise ValueError('Damaged replacement translation catalog')
    rows=[json.loads(line) for line in replacements.decode('utf8').splitlines()]
    # The directory is created here under the caller's checked output folder;
    # cleanup owns this exact temporary directory and never an input path.
    with tempfile.TemporaryDirectory(prefix='snatcher-story-',dir=temporary_parent) as scratch:
        graph=build(source,rows,Path(scratch)/'derived')
        restored=restore_tables(payload,description['records'],control_arrays(graph))
        if 'search' in description:
            from restore_story_search import restore
            restored=restore(restored,description['search'],graph)
        if 'lookup_index' in description:
            from derive_story_index import restore as restore_index
            spans=[(r['offset'],r['offset']+r['size']) for r in description['records']]
            if 'search' in description:
                search=description['search']
                spans.extend((r['offset'],r['offset']+r['size']) for r in [search['table'],*search['strings']])
            restored=restore_index(restored,description['lookup_index'],graph,spans)
        return restored
