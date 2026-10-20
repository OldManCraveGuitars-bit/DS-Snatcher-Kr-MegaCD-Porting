"""Recreate the optimized VM's immutable PC ranges from the owned-disc graph."""
import hashlib
import struct


LAYOUT = 'module_and_64_byte_pc_range'


def build(graph):
    nodes = graph['nodes']
    if not 0 < len(nodes) < 65536:
        raise ValueError('Unsupported indexed gameplay node count')
    buckets = {}
    for i, node in enumerate(nodes):
        module, pc, op = (node[key] for key in ('module_id', 'pc', 'op'))
        if not (type(module) is type(pc) is type(op) is int
                and 0 <= module < 128 and 0 <= pc < 16384 and 0 <= op < 512):
            raise ValueError('Unsupported indexed gameplay key')
        if op < 256:
            key = module*256 + pc//64
            if key not in buckets:
                buckets[key] = [i, i]
            else:
                buckets[key][1] = i
    rows = [(key, *bounds) for key, bounds in sorted(buckets.items())]
    if not rows:
        raise ValueError('Empty gameplay PC index')
    raw = b''.join(struct.pack('<3H', *row) for row in rows)
    header = '''#pragma once
/* Derived node ranges only. Constants occupy ARM9 read-only data, not heap. */
_Static_assert(STORY_NODE_COUNT==%d,"Regenerate the story lookup index");
typedef struct {uint16_t key,first,last;} StoryPcRange;
_Static_assert(sizeof(StoryPcRange)==6,"Story PC range layout");
static const StoryPcRange story_pc_index[]={
''' % len(nodes)
    header += ''.join('{%d,%d,%d},\n' % row for row in rows)
    header += '};\n#define STORY_PC_INDEX_COUNT ' + str(len(rows)) + 'u\n'
    return raw, header


def binding(description, graph):
    if set(description) != {'layout', 'offset', 'size', 'sha256'} or description['layout'] != LAYOUT:
        raise ValueError('Unsupported gameplay PC index layout')
    raw, header = build(graph)
    at, size = description['offset'], description['size']
    if (type(at) is not int or at < 512 or type(size) is not int
            or size != len(raw) or hashlib.sha256(raw).hexdigest() != description['sha256']):
        raise ValueError('Unsupported derived gameplay PC index')
    return at, raw, header


def restore(payload, description, graph, forbidden=()):
    at, raw, _ = binding(description, graph)
    rom, _, ram, size = struct.unpack_from('<4I', payload, 0x20)
    if not rom <= at < at+len(raw) <= min(len(payload), rom+size):
        raise ValueError('Gameplay PC index outside ARM9')
    if any(start < at+len(raw) and at < end for start, end in forbidden):
        raise ValueError('Gameplay PC index overlaps another derived table')
    if any(payload[at:at+len(raw)]):
        raise ValueError('Gameplay PC index region must be erased')
    result = bytearray(payload)
    result[at:at+len(raw)] = raw
    return bytes(result)
