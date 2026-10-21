"""Offline Windows GUI for the v0.9 NitroFS compatibility patcher."""
from pathlib import Path
from contextlib import redirect_stdout, redirect_stderr
from datetime import datetime
import argparse
import faulthandler
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import traceback

sys.dont_write_bytecode = True
APP_VERSION = '0.9 / 한국어 0.9'
MANIFEST_SHA = 'ef474a3f2cda895c831344ce153b548087bf15c9bf8aaa04820a7d64a66d8d88'
NDS_SHA = '5e5a3f3d05374b196ac4799597afb81e5dae9d61e655ae6275cc912260cc3f71'
BIOS_SHA = 'b1c2036b79467514eaaa69c76e5ba83801621e821059caa53cb921a0b21e3af4'


def bundle_root():
    if getattr(sys, 'frozen', False):
        return Path(sys._MEIPASS)
    return Path(__file__).resolve().parent


def payload_root():
    if getattr(sys, 'frozen', False):
        return bundle_root() / 'payload'
    project = Path(__file__).resolve().parents[2]
    return project / 'source-kit'


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(4 * 1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def write_json(path, value):
    tmp = path.with_suffix('.tmp')
    tmp.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf8')
    os.replace(tmp, path)


def validate_inputs(source, bios, output):
    """Cheap user-facing checks, before the converter creates any output."""
    if not source or not bios or not output:
        raise ValueError('원본 ROM(CUE), BIOS, 저장 위치를 모두 선택하세요.')
    source, bios, output = map(lambda x: Path(x).expanduser().resolve(), (source, bios, output))
    if not source.is_file() or source.suffix.lower() != '.cue':
        raise ValueError('수정되지 않은 북미판 ROM의 .cue 파일을 선택하세요.\nBIN 단독, NDS, ZIP, CHD는 입력할 수 없습니다.')
    if not bios.is_file() or bios.stat().st_size != 131072 or sha(bios) != BIOS_SHA:
        raise ValueError('지원하는 북미판 Sega CD BIOS가 아닙니다.\n사용 방법에 적힌 BIOS SHA-256을 확인하세요.')
    if output.exists():
        raise ValueError('결과 폴더가 이미 있습니다. 기존 파일을 덮어쓰지 않으므로 새 이름을 사용하세요.')
    if not output.parent.is_dir():
        raise ValueError('저장할 상위 폴더가 없습니다. 저장 위치를 다시 선택하세요.')
    for protected in (source.parent, bios.parent, payload_root().resolve()):
        if output == protected or output.is_relative_to(protected):
            raise ValueError('원본 ROM·BIOS가 있는 폴더 밖에 별도 저장 위치를 선택하세요.')
    text = source.read_text(encoding='utf-8-sig')
    files = re.findall(r'^\s*FILE\s+"([^"]+)"\s+BINARY\s*$', text, re.I | re.M)
    if not files:
        raise ValueError('CUE에 BINARY 트랙 파일 정보가 없습니다.')
    for name in files:
        if not (source.parent / name).is_file():
            raise ValueError('CUE가 참조하는 BIN 파일이 없습니다:\n' + name)
    tracks = [int(n) for n in re.findall(r'^\s*TRACK\s+(\d+)\s+\S+', text, re.I | re.M)]
    if tracks != list(range(1, 22)):
        raise ValueError('원본의 1~21번 트랙이 모두 필요합니다. 전체 CUE/BIN 구성을 확인하세요.')
    if shutil.disk_usage(output.parent).free < 2 * 1024**3:
        raise ValueError('저장 드라이브에 여유 공간이 2GB 이상 필요합니다.')
    return source, bios, output


def describe_stage(line):
    label = line.removeprefix('Converted:').strip()
    if label.startswith('original FM score'):
        return 'BGM 변환 · ' + label.split('score', 1)[1].strip()
    if label.startswith('original CDDA track'):
        return 'CD 음악 변환 · 트랙 ' + label.split('track', 1)[1].strip()
    groups = [('opening', '오프닝 연출 변환'), ('speech', '음성 데이터 변환'),
              ('PCM', '음성·효과음 변환'), ('portrait', '초상화 변환'),
              ('battle', '전투 화면 변환'), ('startup', '시작 로고·사운드 변환'),
              ('translation', '한국어 적용 및 최종 NDS 검사'), ('casing', '화면 UI 구성')]
    for key, title in groups:
        if key.lower() in label.lower():
            return title
    return '게임 리소스 변환'


class ProgressLog(io.TextIOBase):
    def __init__(self, log, events):
        self.log, self.events, self.pending = log, events, ''
        self.steps = 0

    def write(self, value):
        self.log.write(value)
        self.log.flush()
        self.pending += value
        while '\n' in self.pending:
            line, self.pending = self.pending.split('\n', 1)
            if line.startswith('Converted:'):
                self.steps += 1
                self.events.write(json.dumps({'stage': describe_stage(line), 'steps': self.steps,
                                              'detail': line}, ensure_ascii=False) + '\n')
                self.events.flush()
        return len(value)

    def flush(self):
        self.log.flush()


def worker(job):
    """The GUI and command-line tests run this same frozen worker process."""
    job = Path(job).resolve(strict=True)
    request = json.loads((job / 'request.json').read_text(encoding='utf8'))
    finished = threading.Event()
    # A separate watchdog also interrupts long native converter calls on cancel.
    def watch_cancel():
        while not finished.wait(.2):
            if (job / 'cancel').exists():
                os._exit(130)
    threading.Thread(target=watch_cancel, daemon=True).start()
    started = time.monotonic()
    result = {'ok': False, 'app_version': APP_VERSION, 'frozen': bool(getattr(sys, 'frozen', False))}
    output = None
    with (job/'worker.log').open('w', encoding='utf8', buffering=1) as log, \
            (job/'events.jsonl').open('w', encoding='utf8', buffering=1) as events:
        # Unicorn deliberately handles first-chance Windows exceptions during
        # initialization. Only diagnostic runs should label them in the log.
        if request.get('native_fault_trace'):
            faulthandler.enable(file=log, all_threads=True)
        stream = ProgressLog(log, events)
        try:
            root = payload_root().resolve(strict=True)
            if sha(root/'package-files.json') != MANIFEST_SHA:
                raise ValueError('내장 패처 파일이 손상되었습니다. EXE를 다시 받으세요.')
            sys.path.insert(0, str(root/'patcher'))
            # Do not inherit developer overrides or user-installed modules.
            os.environ.pop('SNATCHER_FORCE_SOURCE', None)
            import patch_release
            with redirect_stdout(stream), redirect_stderr(stream):
                if request.get('check_only'):
                    patch_release.run(['--check'])
                    import tkinter as tk
                    ui_root = tk.Tk()
                    ui_root.withdraw()
                    try:
                        ui = PatcherApp(ui_root)
                        ui_root.update_idletasks()
                        result.update(ok=True, check_only=True, gui_title=ui_root.title(),
                                      gui_requested_size=[ui_root.winfo_reqwidth(), ui_root.winfo_reqheight()],
                                      gui_start_enabled=str(ui.start_button['state']) == 'normal')
                    finally:
                        ui_root.destroy()
                else:
                    source, bios, output = validate_inputs(request['source'], request['bios'], request['output'])
                    patch_release.run(['--source', str(source), '--bios', str(bios), '--output', str(output)])
                    final = json.loads((output/'patch-result.json').read_text(encoding='utf8'))
                    actual = sha(output/'snatcher_ds.nds')
                    if actual != NDS_SHA or final['nds_sha256'] != actual:
                        raise ValueError('최종 NDS 검증에 실패했습니다. 생성된 파일을 사용하지 마세요.')
                    result.update(ok=True, output=str(output), nds=str(output/'snatcher_ds.nds'),
                                  nds_sha256=actual, resource_files=final['resource_files'])
        except Exception as error:
            result.update(error=str(error), error_type=type(error).__name__)
            traceback.print_exc(file=log)
        finally:
            result.update(elapsed_seconds=round(time.monotonic()-started, 2), stages=stream.steps)
            write_json(job/'result.json', result)
            finished.set()
            faulthandler.disable()
    if output is not None and output.is_dir():
        shutil.copyfile(job/'worker.log', output/'patcher.log')
        shutil.copyfile(job/'result.json', output/'exe-result.json')
    return 0 if result['ok'] else 1


def launch_worker(job):
    if getattr(sys, 'frozen', False):
        command = [sys.executable, '--worker', str(job)]
    else:
        command = [sys.executable, '-I', '-X', 'utf8', '-B', str(Path(__file__).resolve()), '--worker', str(job)]
    env = os.environ.copy()
    for key in ('PYTHONPATH', 'PYTHONHOME', 'SNATCHER_FORCE_SOURCE'):
        env.pop(key, None)
    return subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW, env=env)


class PatcherApp:
    def __init__(self, root, preset=None):
        import tkinter as tk
        from tkinter import ttk
        self.root, self.tk, self.ttk = root, tk, ttk
        self.process = self.job = self.output = None
        self.closing = False
        self.offset = 0
        root.title('SNATCHER DS 패처 v0.9 · 한국어 0.9')
        root.geometry('800x650')
        root.minsize(740, 620)
        root.option_add('*Font', ('맑은 고딕', 10))
        style = ttk.Style(root)
        style.theme_use('vista')
        style.configure('Title.TLabel', font=('맑은 고딕', 22, 'bold'))
        style.configure('Sub.TLabel', foreground='#52606d')
        style.configure('Status.TLabel', font=('맑은 고딕', 11, 'bold'))
        style.configure('TButton', padding=(12, 7))
        base = ttk.Frame(root, padding=24)
        base.pack(fill='both', expand=True)
        base.columnconfigure(0, weight=1)
        base.rowconfigure(8, weight=1)
        ttk.Label(base, text='SNATCHER DS', style='Title.TLabel').grid(row=0, column=0, sticky='w')
        ttk.Label(base, text='원본 ROM + BIOS → 한국어 NDS  |  EXE 패처 v0.9',
                  style='Sub.TLabel').grid(row=1, column=0, sticky='w', pady=(2, 18))
        form = ttk.Frame(base)
        form.grid(row=2, column=0, sticky='ew')
        form.columnconfigure(1, weight=1)
        self.values = [tk.StringVar(value=v) for v in (preset or ('', '', ''))]
        self.controls = []
        for i, label in enumerate(('원본 ROM (.cue)', 'Sega CD BIOS', '저장 위치')):
            ttk.Label(form, text=label, width=17).grid(row=i, column=0, sticky='w', pady=7)
            entry = ttk.Entry(form, textvariable=self.values[i])
            entry.grid(row=i, column=1, sticky='ew', padx=(0, 10), pady=7)
            button = ttk.Button(form, text='찾아보기…', command=lambda i=i: self.browse(i))
            button.grid(row=i, column=2, pady=7)
            self.controls.extend((entry, button))
        ttk.Label(base, text='북미판 원본 CUE와 모든 BIN 트랙이 필요합니다. 한글 0.9은 자동 적용됩니다.\n'
                  '선택한 저장 위치에 새 결과 폴더를 만듭니다. Python 설치는 필요 없습니다.',
                  style='Sub.TLabel').grid(row=3, column=0, sticky='w', pady=(9, 16))
        actions = ttk.Frame(base)
        actions.grid(row=4, column=0, sticky='ew')
        self.start_button = ttk.Button(actions, text='NDS 만들기', command=self.start)
        self.start_button.pack(side='left')
        self.controls.append(self.start_button)
        self.cancel_button = ttk.Button(actions, text='취소', command=self.cancel, state='disabled')
        self.cancel_button.pack(side='left', padx=8)
        ttk.Button(actions, text='사용 방법', command=self.help).pack(side='right')
        self.status = tk.StringVar(value='원본 ROM과 BIOS를 선택해 주세요.')
        ttk.Label(base, textvariable=self.status, style='Status.TLabel', wraplength=730).grid(
            row=5, column=0, sticky='w', pady=(22, 7))
        self.progress = ttk.Progressbar(base, mode='indeterminate')
        self.progress.grid(row=6, column=0, sticky='ew')
        self.time_label = tk.StringVar(value='준비됨 · 모든 변환은 이 컴퓨터에서 진행됩니다.')
        ttk.Label(base, textvariable=self.time_label, style='Sub.TLabel').grid(row=7, column=0, sticky='w', pady=7)
        logs = ttk.Frame(base)
        logs.grid(row=8, column=0, sticky='nsew')
        self.log = tk.Text(logs, height=6, wrap='word', state='disabled', relief='solid', borderwidth=1,
                           background='#f5f7fa', foreground='#263442', font=('맑은 고딕', 9))
        scrollbar = ttk.Scrollbar(logs, command=self.log.yview)
        scrollbar.pack(side='right', fill='y')
        self.log.configure(yscrollcommand=scrollbar.set)
        self.log.pack(fill='both', expand=True)
        bottom = ttk.Frame(base)
        bottom.grid(row=9, column=0, sticky='ew', pady=(12, 8))
        self.folder_button = ttk.Button(bottom, text='결과 폴더 열기', command=self.open_output, state='disabled')
        self.folder_button.pack(side='left')
        self.log_button = ttk.Button(bottom, text='로그 열기', command=self.open_log, state='disabled')
        self.log_button.pack(side='left', padx=8)
        ttk.Label(base, text='버그·미검증 선택 분기와 일부 소리 끊김이 남아 있습니다.\n'
                  '원본 게임과 BIOS는 포함되지 않습니다. 기존 공개 Demo 패처는 변경하지 않습니다.',
                  style='Sub.TLabel').grid(row=10, column=0, sticky='w')
        root.protocol('WM_DELETE_WINDOW', self.close)

    def browse(self, index):
        from tkinter import filedialog
        if index == 2:
            value = filedialog.askdirectory(parent=self.root, title='결과를 저장할 위치')
        else:
            value = filedialog.askopenfilename(parent=self.root, title=('원본 북미판 CUE', '북미판 Sega CD BIOS')[index],
                    filetypes=[('CUE 파일', '*.cue')] if index == 0 else [('BIOS 파일', '*.bin'), ('모든 파일', '*.*')])
        if value:
            self.values[index].set(value)

    def append(self, value):
        self.log.configure(state='normal')
        self.log.insert('end', value + '\n')
        self.log.see('end')
        self.log.configure(state='disabled')

    def help(self):
        from tkinter import messagebox
        messagebox.showinfo('사용 방법', '1. 수정되지 않은 북미판 Snatcher .cue 파일을 선택하세요.\n'
            '   CUE가 참조하는 모든 BIN 파일도 필요합니다.\n2. 지원하는 북미판 Sega CD BIOS를 선택하세요.\n'
            '3. 원본과 다른 폴더를 저장 위치로 선택하고 [NDS 만들기]를 누르세요.\n'
            '4. 완료 후 snatcher_ds.nds를 SD 카드로 복사하세요.\n\n'
            '변환에는 수 분이 걸립니다. 저장 드라이브에 2GB 이상 여유 공간을 준비하세요.\n'
            '기존 snatcher_ds 저장 폴더는 먼저 백업하고 유지하세요.\n\n'
            '지원 BIOS SHA-256:\n' + BIOS_SHA, parent=self.root)

    def start(self):
        from tkinter import messagebox
        if self.process is not None:
            return
        try:
            source, bios, parent = [v.get().strip().strip('"') for v in self.values]
            if not all((source, bios, parent)):
                raise ValueError('원본 ROM(CUE), BIOS, 저장 위치를 모두 선택하세요.')
            stamp = datetime.now().strftime('%Y%m%d-%H%M%S-%f')
            output = Path(parent) / ('Snatcher-DS-' + stamp)
            source, bios, output = validate_inputs(source, bios, output)
            self.job = Path(tempfile.mkdtemp(prefix='SnatcherDS-Patcher-'))
            write_json(self.job/'request.json', dict(source=str(source), bios=str(bios), output=str(output)))
            self.process = launch_worker(self.job)
            self.output, self.started, self.offset = output, time.monotonic(), 0
            self.status.set('패처 검사 및 원본 데이터 준비 중…')
            self.append('결과 폴더: ' + str(output))
            for control in self.controls:
                control.configure(state='disabled')
            self.folder_button.configure(state='disabled')
            self.log_button.configure(state='disabled')
            self.cancel_button.configure(state='normal')
            self.progress.configure(mode='indeterminate')
            self.progress.start(15)
            self.root.after(200, self.poll)
        except Exception as error:
            messagebox.showerror('입력 확인', str(error), parent=self.root)

    def poll(self):
        if self.process is None:
            return
        events = self.job/'events.jsonl'
        if events.exists():
            with events.open(encoding='utf8') as f:
                f.seek(self.offset)
                while True:
                    before = f.tell()
                    line = f.readline()
                    if not line.endswith('\n'):
                        f.seek(before)
                        break
                    event = json.loads(line)
                    self.status.set(event['stage'])
                    self.append(f"{event['steps']:02d}. {event['stage']}")
                self.offset = f.tell()
        seconds = int(time.monotonic()-self.started)
        self.time_label.set(f'경과 시간 {seconds//60}분 {seconds%60:02d}초 · 진행 막대는 남은 시간을 뜻하지 않습니다.')
        code = self.process.poll()
        if code is None:
            self.root.after(250, self.poll)
            return
        self.process = None
        self.progress.stop()
        self.cancel_button.configure(state='disabled')
        for control in self.controls:
            control.configure(state='normal')
        result_path = self.job/'result.json'
        result = json.loads(result_path.read_text(encoding='utf8')) if result_path.exists() else {}
        write_json(self.job/'gui-outcome.json', {'exit_code': code, 'result': result,
                   'cancel_requested': (self.job/'cancel').exists()})
        if code == 0 and result.get('ok'):
            self.progress.configure(mode='determinate', value=100)
            self.status.set('완료 · snatcher_ds.nds 생성 및 검증 성공')
            self.append('완료: ' + result['nds'])
            self.append('SHA-256: ' + result['nds_sha256'])
        elif (self.job/'cancel').exists():
            self.status.set('취소됨 · 미완성 .partial 파일은 실행하지 마세요.')
            self.append('변환을 취소했습니다. 새로 시작하면 별도 결과 폴더를 만듭니다.')
            write_json(self.job/'cancel-result.json', {'cancelled': True, 'exit_code': code})
        else:
            self.status.set('변환 실패 · 로그를 확인해 주세요.')
            self.append(result.get('error', f'변환 프로세스가 종료되었습니다. 코드: {code}'))
        if self.output.exists():
            self.folder_button.configure(state='normal')
            if (self.job/'worker.log').exists():
                shutil.copyfile(self.job/'worker.log', self.output/'patcher.log')
        if (self.job/'worker.log').exists():
            self.log_button.configure(state='normal')
        if self.closing:
            self.root.destroy()

    def cancel(self, closing=False):
        from tkinter import messagebox
        if self.process and messagebox.askyesno('변환 취소', '진행 중인 변환을 취소할까요?\n'
                '원본은 변경되지 않으며, 미완성 결과와 로그는 남습니다.', parent=self.root):
            self.closing = closing
            (self.job/'cancel').touch()
            self.cancel_button.configure(state='disabled')
            self.status.set('변환을 취소하는 중…')

    def close(self):
        if self.process:
            self.cancel(closing=True)
        else:
            self.root.destroy()

    def open_output(self):
        if self.output and self.output.is_dir():
            os.startfile(str(self.output))

    def open_log(self):
        if self.job:
            path = self.output/'patcher.log' if self.output and (self.output/'patcher.log').exists() else self.job/'worker.log'
            if path.exists():
                os.startfile(str(path))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--worker', help=argparse.SUPPRESS)
    parser.add_argument('--source')
    parser.add_argument('--bios')
    parser.add_argument('--destination')
    args = parser.parse_args()
    if args.worker:
        return worker(args.worker)
    import tkinter as tk
    root = tk.Tk()
    PatcherApp(root, (args.source or '', args.bios or '', args.destination or ''))
    root.mainloop()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
