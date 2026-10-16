"""Decode the eight original SP18 phone voices from the user's disc."""
from convert_headquarters_speech import decode

IDENTIFIERS = (1185, 1186, 1187, 1188, 1189, 1190, 1191, 1193)


def build(source):
    return decode(source, IDENTIFIERS)
