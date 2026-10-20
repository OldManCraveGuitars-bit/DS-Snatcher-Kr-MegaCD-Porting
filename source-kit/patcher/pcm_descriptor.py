"""Bounds for the original eight-byte PCM load descriptors.

SUB154F8 indexes a 16-bit ID at PCMLT+ID*8. The first byte of the
dummy record is not a complete entry count (1197 valid IDs in the USA
disc extend beyond its value174). Effects use a separate table at4000.
"""
import struct


def descriptor(table, identifier):
    if not isinstance(identifier, int) or not 0 < identifier < 0x4000 // 8:
        raise ValueError('Invalid original PCM load ID')
    at = identifier * 8
    if at + 8 > len(table):
        raise ValueError('Truncated original PCM load table')
    value = table[at:at + 8]
    if value[1] not in (1, 2, 0x82) or not struct.unpack_from('>H', value, 6)[0]:
        raise ValueError('Missing or unsupported original PCM descriptor')
    return value
