"""Copy encoded speech extents from the user's disc for native PCM playback.

No rendered audio, recorded timing or emulator state is an input. The original
PCMLT descriptors still control volume, rate, streaming and termination.
"""
import struct
from native_resources import sha
from pcm_descriptor import descriptor as load_descriptor


def build(source, identifiers, *, allow_effect_banks=False):
    table = source.read_file('PCMLT_01.BIN')
    data = source.read_file('PCMLD_01.BIN')
    ids = tuple(identifiers)
    if not ids or len(set(ids)) != len(ids):
        raise ValueError('Supply distinct original speech identifiers')
    outputs, rows = {}, []
    for identifier in ids:
        descriptor = load_descriptor(table, identifier)
        effect_bank = descriptor[1] == 1 and allow_effect_banks
        if descriptor[1] not in (2, 0x82) and not effect_bank:
            raise ValueError('The requested PCM entry is not original speech')
        sector, count = struct.unpack_from('>HH', descriptor, 4)
        if not count or (sector + count) * 2048 > len(data):
            raise ValueError('Invalid original speech extent')
        if effect_bank and count >= 53:
            raise ValueError('Original effect bank exceeds native PCM storage')
        payload = data[sector * 2048:(sector + count) * 2048]
        name = f'native-pcm-bank-{identifier:04d}-r22.bin' if effect_bank else f'native-speech-{identifier:04d}-r22.bin'
        outputs[name] = payload
        rows.append(dict(name=name, identifier=identifier, source_sector=sector,
                         sectors=count, bytes=len(payload), sha256=sha(payload),
                         descriptor=descriptor.hex()))
    return outputs, dict(source='PCMLD_01.BIN', table_sha256=sha(table),
                         outputs=rows, captured_inputs=False, source_writes=0)
