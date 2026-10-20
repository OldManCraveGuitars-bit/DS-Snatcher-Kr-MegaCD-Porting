"""Validate source extents using size/SHA-256 metadata, not copied game bytes."""
import hashlib


def _descriptor(size, digest):
    if type(size) is not int or size <= 0 or not isinstance(digest, str):
        raise ValueError('Invalid source fingerprint')
    if len(digest) != 64 or any(ch not in '0123456789abcdef' for ch in digest):
        raise ValueError('Invalid source fingerprint')


def matches_fingerprint(data, size, digest):
    _descriptor(size, digest)
    return len(data) == size and hashlib.sha256(data).hexdigest() == digest


def fingerprint_positions(data, size, digest, start=0):
    _descriptor(size, digest)
    if type(start) is not int or not 0 <= start <= len(data):
        raise ValueError('Invalid source search start')
    view = memoryview(data)
    return [at for at in range(start, len(data) - size + 1)
            if hashlib.sha256(view[at:at + size]).hexdigest() == digest]


def find_fingerprint(data, size, digest, start=0):
    found = fingerprint_positions(data, size, digest, start)
    return found[0] if found else -1


def unique_fingerprint(data, size, digest, message):
    found = fingerprint_positions(data, size, digest)
    if len(found) != 1:
        raise ValueError(message)
    return found[0]
