"""Derive keyboard title references from stable original text identities."""

TITLE_SOURCES = (
    ('PEOPLE_NAME_TEXT', 'SP05.BIN/0018', 0x3e),
    ('PHONE_NAME_TEXT', 'SP18.BIN/0010', 0x4b),
    ('PASSWORD_NAME_TEXT', 'SP18.BIN/0004', 0x4b),
    ('GIBSON_ANSWER_TEXT', 'SP09.BIN/0008', 0x42),
)


def title_bindings(graph):
    texts = graph['texts']
    result = {}
    for macro, entry, module in TITLE_SOURCES:
        matches = [i for i, row in enumerate(texts) if row['entry_id'] == entry]
        if len(matches) != 1 or texts[matches[0]]['module_id'] != module:
            raise ValueError('Missing, duplicate or mismatched keyboard title: ' + entry)
        result[macro] = matches[0]
    return result


def source_header(graph):
    lines = ['#pragma once', '/* Rebound from original text IDs after graph conversion. */']
    lines += ['#define ' + name + ' ' + str(index)
              for name, index in title_bindings(graph).items()]
    return '\n'.join(lines) + '\n'
