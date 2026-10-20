"""Obtain native speech mode/envelope/pan from the user's PCM descriptor table."""


def build(source):
    table=source.read_file('PCMLT_01.BIN')
    if len(table)<2048*8:raise ValueError('Truncated original PCM descriptor table')
    return b''.join(table[i*8+1:i*8+4] for i in range(2048))


def source_header(source):
    data=build(source)
    rows=['{%d,%d,%d}'%tuple(data[i:i+3]) for i in range(0,len(data),3)]
    return '#pragma once\n#include <stdint.h>\nstatic const uint8_t voice_source_gain[2048][3]={\n'+',\n'.join(rows)+'\n};\n'
