"""Restore compiled original search keys exclusively from the owned-disc graph."""
import hashlib,struct
from derive_story_graph import search_entries


def restore(payload,description,graph):
    entries=search_entries(graph)
    texts={hashlib.sha256(data).hexdigest():data for _,data in entries}
    locations=description['strings'];table=description['table']
    if not isinstance(locations,list) or len(locations)!=len(texts):raise ValueError('Incomplete original search strings')
    rom_at,_,ram_at,arm_bytes=struct.unpack_from('<4I',payload,0x20)
    by_hash={};writes={}
    def checked_span(at,data):
        if type(at) is not int or not rom_at<=at<at+len(data)<=min(len(payload),rom_at+arm_bytes):
            raise ValueError('Search binding outside ARM9')
        for address,value in enumerate(data,at):
            if address in writes and writes[address]!=value:raise ValueError('Inconsistent pooled search strings')
            writes[address]=value
    for row in locations:
        h=row['sha256'];at=row['offset']
        if h not in texts or h in by_hash or row['size']!=len(texts[h]):raise ValueError('Unsupported original search string binding')
        checked_span(at,texts[h]);by_hash[h]=at
    raw=b''.join(struct.pack('<II',key,ram_at+by_hash[hashlib.sha256(data).hexdigest()]-rom_at) for key,data in entries)
    if len(raw)!=table['size'] or hashlib.sha256(raw).hexdigest()!=table['sha256']:
        raise ValueError('Unsupported original search pointer table')
    at=table['offset']
    if type(at) is not int or any(at<=address<at+len(raw) for address in writes):
        raise ValueError('Search pointer table overlaps strings')
    checked_span(at,raw)
    if any(payload[address] for address in writes):raise ValueError('Original search regions must be erased in the template')
    result=bytearray(payload)
    for address,value in writes.items():result[address]=value
    return bytes(result)
