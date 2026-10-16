"""Original portrait maps selected by the user's DS battle presentation."""
from convert_portraits import build as build_portraits


def build(source, bindings):
    frames = (14, 69, 68, 67)
    rows = []
    for index, original in enumerate(frames):
        row = dict(bindings['frames'][original])
        row['frame'] = index
        rows.append(row)
    packed, report = build_portraits({'frames': rows}, source)
    report['health_selection'] = list(frames)
    report['adaptation'] = 'User-selected source expressions for health; zero-health grayscale is applied at runtime.'
    return {'native-battle-faces-r22.nspr': packed}, report
