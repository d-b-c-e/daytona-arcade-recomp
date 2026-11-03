"""Own-job root. Only invoked by the suspended-root controller after live GO."""
import ctypes, ctypes.wintypes as wt, hashlib, json, pathlib, subprocess, sys, time, traceback
from shared import gate, progress, read

user = ctypes.WinDLL('user32', use_last_error=True)
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
user.PostMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
user.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user.IsWindowVisible.argtypes = [wt.HWND]; user.IsWindowVisible.restype = wt.BOOL
user.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
kernel.QueryFullProcessImageNameW.argtypes = [wt.HANDLE, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD)]
kernel.GetProcessTimes.argtypes = [wt.HANDLE] + [ctypes.POINTER(wt.FILETIME)] * 4

def identity(process, exe):
    h = wt.HANDLE(int(process._handle)); name = ctypes.create_unicode_buffer(32768); size = wt.DWORD(len(name))
    if not kernel.QueryFullProcessImageNameW(h, 0, name, ctypes.byref(size)):
        raise ctypes.WinError(ctypes.get_last_error())
    times = [wt.FILETIME() for _ in range(4)]
    if not kernel.GetProcessTimes(h, *(ctypes.byref(t) for t in times)):
        raise ctypes.WinError(ctypes.get_last_error())
    birth = (times[0].dwHighDateTime << 32 | times[0].dwLowDateTime) + 504911232000000000
    if pathlib.Path(name.value).resolve() != exe.resolve():
        raise RuntimeError('exact game image differs')
    return dict(pid=process.pid, birth=birth, image=name.value)

def window_owned(hwnd, pid):
    owner = wt.DWORD(); user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
    name = ctypes.create_unicode_buffer(256); user.GetClassNameW(hwnd, name, len(name))
    return owner.value == pid and user.IsWindowVisible(hwnd) and name.value == 'SDL_app'

def run(plan, live):
    state = live / 'supervisor.json'; private = live / 'state'; private.mkdir()
    exe = pathlib.Path(plan['candidate']); cap = plan['capture']; session = plan['session']
    game = None; capture = None; hwnd = None; failure = None; rows = None; bursts = []
    def dump(name, data): (live / name).write_text(json.dumps(data, indent=1))
    def closing():
        (live / 'supervisor.closing').touch()
        end = time.monotonic() + 3
        while time.monotonic() < end:
            try:
                if json.loads(read(state)).get('cleanup'): break
            except FileNotFoundError: pass
            time.sleep(.02)
        else: raise RuntimeError('parent cleanup handshake absent')
        (private / 'cleanup.ok').write_text(session)
    try:
        gate(state)
        with (live / 'game-stdout.txt').open('wb') as output:
            game = subprocess.Popen([str(exe), '--rom', plan['rom'], '--autostart', '--frames', '1800',
                '--readback-state', str(private), '--readback-session', session], cwd=exe.parent,
                stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW)
        who = identity(game, exe); dump('worker-game.json', who)
        begin = time.monotonic(); sequence = -1; next_burst = 600; last_frame = 0; last_advance = begin
        while game.poll() is None:
            supervisor = gate(state, require_cleared=False) # pending raw startup resets inhibit captures; parent resolves legality
            if time.monotonic() - begin > 150: raise RuntimeError('startup/guest completion deadline exceeded')
            if (private / 'readback.jsonl').exists():
                rows = progress(read(private / 'readback.jsonl'), session, sequence)
                if rows:
                    sequence = rows['sequence']
                    if rows['frames'] > last_frame: last_frame = rows['frames']; last_advance = time.monotonic()
                    if rows['frames'] and time.monotonic() - last_advance > 5: raise RuntimeError('guest frame progression stopped')
                    if rows['frames'] and not rows['hardware']: raise RuntimeError('hardware renderer witness absent')
                    if rows['phase'] == 'closing': closing(); break
                    if rows['frames'] >= next_burst and capture is None and len(bursts) < 2 and not supervisor.get('pending'):
                        # Enumerator itself is an owned child, never desktop capture or foreground activation.
                        listing = subprocess.run([cap, str(game.pid), '--list'], capture_output=True, text=True, timeout=5, creationflags=subprocess.CREATE_NO_WINDOW)
                        if listing.returncode: raise RuntimeError('render-window enumeration failed: ' + listing.stderr)
                        record = json.loads(listing.stdout); dump(f'windows-{next_burst}.json', record)
                        windows = [w for w in record['windows'] if w['Class'] == 'SDL_app' and w['Title'] == 'Daytona USA']
                        if len(windows) != 1: raise RuntimeError('one exact SDL render window required')
                        hwnd = windows[0]['Handle']
                        if not window_owned(hwnd, game.pid): raise RuntimeError('render HWND owner/class/visibility refused')
                        folder = live / f'capture-{next_burst}'
                        capture = subprocess.Popen([cap, str(game.pid), str(folder), '--hwnd', hex(hwnd), '--duration', '2', '--max-frames', '3'],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW)
                        bursts.append(folder); next_burst += 600
            if capture is not None and capture.poll() is not None:
                if capture.returncode: raise RuntimeError('R2 capture helper failed')
                capture = None
            time.sleep(.05)
        if game.poll() is None:
            game.wait(timeout=15)
        rows = progress(read(private / 'readback.jsonl'), session, sequence)
        if game.returncode != 0 or rows['phase'] != 'complete' or rows['frames'] != 1800 or rows['presents'] < 1:
            raise RuntimeError('terminal exact-frame/readback/exit assertion failed')
        if capture is not None: capture.wait(timeout=10)
        if len(bursts) != 2: raise RuntimeError('two distinct capture intervals required')
        for folder in bursts:
            record = json.loads((folder / 'capture.json').read_text())
            if record['status'] != 'complete' or not 1 <= record['saved'] <= 3: raise RuntimeError('R2 incomplete or empty')
            for frame in record['frames']:
                if hashlib.sha256((folder / frame['File']).read_bytes()).hexdigest().lower() != frame['Sha256'].lower():
                    raise RuntimeError('captured frame hash differs')
    except BaseException as e:
        failure = str(e); (live / 'worker-error.txt').write_text(traceback.format_exc())
    finally:
        try: closing()
        except BaseException as e: failure = f'{failure}; cleanup: {e}'
        if game is not None and game.poll() is None and hwnd and window_owned(hwnd, game.pid):
            user.PostMessageW(hwnd, 0x10, 0, 0)
        if game is not None and game.poll() is None:
            try: game.wait(timeout=30)
            except subprocess.TimeoutExpired: failure = f'{failure}; owned game still alive'
        if capture is not None and capture.poll() is None:
            try: capture.wait(timeout=10)
            except subprocess.TimeoutExpired: failure = f'{failure}; capture still alive'
        dump('worker-result.json', dict(result='fail' if failure else 'pass', failure=failure, last=rows,
            gameExit=None if game is None else game.poll(), captures=[str(p) for p in bursts]))
    if failure: raise RuntimeError(failure)

if __name__ == '__main__':
    plan = json.loads(pathlib.Path(sys.argv[1]).read_text()); live = pathlib.Path(sys.argv[2])
    try: run(plan, live)
    except BaseException: traceback.print_exc(); sys.exit(1)
