"""Exercise GUI callbacks and the actual EXE worker without external Python PATH."""
from pathlib import Path
from unittest.mock import patch
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import tkinter as tk
import argparse
from tkinter import filedialog, messagebox

import patcher_app as app

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT/'work/exe-patcher-v101'
TEST = WORK/'검증 한글 공백 경로'
SOURCE = Path('C:/CODEX/roms/Snatcher_USA/Snatcher (USA).cue')
BIOS = Path('C:/CODEX/bios/us_scd2_9306.bin')
EXE = TEST/'Snatcher-DS-Patcher.exe'


def launch(job):
    env = os.environ.copy()
    env['PATH'] = str(Path(os.environ['SYSTEMROOT'])/'System32')
    env.pop('PYTHONPATH', None)
    env.pop('PYTHONHOME', None)
    env.pop('SNATCHER_FORCE_SOURCE', None)
    return subprocess.Popen([str(EXE), '--worker', str(job)], cwd=TEST, env=env,
                            creationflags=subprocess.CREATE_NO_WINDOW)


def run():
    TEST.mkdir(exist_ok=False)
    shutil.copyfile(WORK/'dist/Snatcher-DS-Patcher.exe', EXE)
    checks = []
    protected = [SOURCE, SOURCE.with_suffix('.bin'), BIOS, app.payload_root()/'package-files.json']
    before = {str(p): app.sha(p) for p in protected}
    result = {'passed': False, 'exe_sha256': app.sha(EXE), 'checks': checks,
              'frozen_worker': True, 'gui_callback_test': True, 'desktop_click_test': False,
              'path': 'Windows System32 only', 'original_before': before}
    root = tk.Tk()
    root.withdraw()
    ui = app.PatcherApp(root)
    ui_errors = []
    root.report_callback_exception = lambda kind, value, trace: ui_errors.append(str(value))
    try:
        check_job = TEST/'standalone-gui-check'
        check_job.mkdir()
        app.write_json(check_job/'request.json', {'check_only': True})
        check_proc = launch(check_job)
        assert check_proc.wait(timeout=90) == 0
        gui_check = json.loads((check_job/'result.json').read_text(encoding='utf8'))
        assert gui_check['ok'] and gui_check['frozen'] and gui_check['gui_start_enabled']
        assert 'SNATCHER DS' in gui_check['gui_title']
        checks.append('Actual frozen EXE loads Tcl/Tk and constructs complete GUI without Python PATH')
        with patch.object(filedialog, 'askopenfilename', side_effect=[str(SOURCE), str(BIOS)]), \
             patch.object(filedialog, 'askdirectory', return_value=str(TEST)):
            ui.browse(0)
            ui.browse(1)
            ui.browse(2)
        assert [v.get() for v in ui.values] == [str(SOURCE), str(BIOS), str(TEST)]
        checks.append('GUI file selectors populate ROM/BIOS/destination')
        with patch.object(messagebox, 'showerror') as error:
            ui.values[0].set('')
            ui.start_button.invoke()
            assert error.call_count == 1 and ui.process is None
        ui.values[0].set(str(SOURCE))
        checks.append('GUI rejects missing input before creating output')
        invalid_bios = TEST/'bad-bios.bin'
        invalid_bios.write_bytes(b'not a BIOS')
        existing = TEST/'already-exists'
        existing.mkdir()
        (existing/'keep.txt').write_text('preserve me', encoding='utf8')
        missing = TEST/'missing-bin.cue'
        missing.write_text('FILE "does-not-exist.bin" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n', encoding='utf8')
        trials = [('bad BIOS', SOURCE, invalid_bios, TEST/'bad-result'),
                  ('existing output', SOURCE, BIOS, existing),
                  ('source directory output', SOURCE, BIOS, SOURCE.parent/'must-not-create'),
                  ('missing BIN', missing, BIOS, WORK/'missing-result')]
        for label, source, bios, output in trials:
            job = TEST/('reject-'+label.replace(' ', '-'))
            job.mkdir()
            app.write_json(job/'request.json', dict(source=str(source), bios=str(bios), output=str(output)))
            proc = launch(job)
            assert proc.wait(timeout=90) != 0
            record = json.loads((job/'result.json').read_text(encoding='utf8'))
            assert not record['ok'] and record['frozen']
            assert output.exists() == (output == existing)
            checks.append('EXE rejects '+label)
        assert (existing/'keep.txt').read_text(encoding='utf8') == 'preserve me'
        checks.append('Existing files retained')
        # Real conversion cancellation while the frozen process is running.
        with patch.object(app, 'launch_worker', side_effect=launch), \
             patch.object(messagebox, 'askyesno', return_value=True):
            ui.start_button.invoke()
            assert ui.process is not None
            cancel_job = ui.job
            deadline = time.monotonic()+90
            while not (cancel_job/'worker.log').exists():
                assert time.monotonic() < deadline
                root.update()
                time.sleep(.1)
            ui.cancel_button.invoke()
            while ui.process:
                assert time.monotonic() < deadline
                root.update()
                time.sleep(.1)
            assert '취소됨' in ui.status.get()
            assert not (ui.output/'snatcher_ds.nds').exists()
            assert str(ui.start_button['state']) == 'normal'
        checks.append('GUI cancel terminates actual EXE worker; no final NDS; retry enabled')
        with patch.object(app, 'launch_worker', side_effect=launch):
            ui.start_button.invoke()
            assert ui.process is not None
            assert str(ui.start_button['state']) == 'disabled'
            assert str(ui.cancel_button['state']) == 'normal'
            result.update(job=str(ui.job), output=str(ui.output))
            app.write_json(TEST/'verification.json', result)
            print('FULL CONVERSION STARTED: '+str(ui.job), flush=True)
            started = time.monotonic()
            deadline = started+1800
            last = started
            updates = 0
            while ui.process:
                assert time.monotonic() < deadline, 'Conversion timed out'
                root.update()
                updates += 1
                time.sleep(.05)
                if time.monotonic()-last > 30:
                    print(ui.status.get()+' | '+ui.time_label.get(), flush=True)
                    last = time.monotonic()
            final = json.loads((ui.job/'result.json').read_text(encoding='utf8'))
            assert final['ok'] and final['frozen'] and final['nds_sha256'] == app.NDS_SHA, final
            assert final['resource_files'] == 1230
            assert '완료' in ui.status.get()
            assert float(ui.progress['value']) == 100
            assert str(ui.folder_button['state']) == 'normal'
            assert str(ui.start_button['state']) == 'normal'
            assert (ui.output/'patcher.log').is_file()
            assert not ui_errors, ui_errors
            checks.append('All 1230 assets and NDS exactly match the v1.01 release reference')
            checks.append('GUI stays responsive; progress, success, log and folder states pass')
            result.update(passed=True, conversion=final, gui_updates=updates,
                          output=str(ui.output), job=str(ui.job), ui_errors=ui_errors)
        after = {str(p): app.sha(p) for p in protected}
        assert before == after
        checks.append('ROM, BIOS and source kit manifest hashes unchanged')
        result['original_after'] = after
    finally:
        root.destroy()
        app.write_json(TEST/'verification.json', result)
    print(json.dumps(result, ensure_ascii=False, indent=2), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--work', default='exe-patcher-v101')
    args = parser.parse_args()
    assert args.work.replace('-', '').isalnum()
    WORK = ROOT/'work'/args.work
    TEST = WORK/'검증 한글 공백 경로'
    EXE = TEST/'Snatcher-DS-Patcher.exe'
    run()
