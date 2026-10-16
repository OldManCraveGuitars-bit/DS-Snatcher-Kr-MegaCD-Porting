"""Decode the original SP08 speech extents for the existing DS PCM stream.

Input is the user's disc only. No waveform recording or emulator state is used.
"""
import hashlib
import re
from convert_native_speech import build as encoded_speech


def decode(source, identifiers):
    """Decode source sign/magnitude speech without narrowing its word ID."""
    encoded, original = encoded_speech(source, identifiers)
    outputs, rows = {}, []
    for record in original['outputs']:
        voice = record['identifier']
        data = encoded[record['name']]
        endings = [m.start() for m in re.finditer(b'\xff{4,}', data)
                   if set(data[m.end():]) <= {0x80}]
        if len(endings) != 1:
            raise ValueError(f'Ambiguous original speech terminator: {voice}')
        # RF5C164 sign/magnitude -> the signed linear DS eight-bit channel.
        decoded = bytes(((128-x) if x < 128 else x) ^ 128
                        for x in data[:endings[0]])
        packed = decoded + bytes((-len(decoded)) % 4)
        name = f'pcm{voice:04d}.bin'
        outputs[name] = packed
        rows.append(dict(record, decoded_name=name, samples=len(decoded),
                         padded_bytes=len(packed),
                         decoded_sha256=hashlib.sha256(packed).hexdigest()))
    return outputs, dict(source=original, voices=rows, captured_inputs=False,
                         source_writes=0, rate_hz=16277)


def build(source):
    return decode(source, range(181, 202))
