"""Windows shared-delete readers and pure production admission rules."""
import ctypes, ctypes.wintypes as wt, json, msvcrt, os, time
kernel = ctypes.WinDLL('kernel32', use_last_error=True)

def read(path):
    create = kernel.CreateFileW
    create.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, wt.LPVOID, wt.DWORD, wt.DWORD, wt.HANDLE]
    create.restype = wt.HANDLE
    h = create(str(path), 0x80000000, 7, None, 3, 0, None)
    if h == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        fd = msvcrt.open_osfhandle(h, os.O_RDONLY | os.O_BINARY)
    except BaseException:
        kernel.CloseHandle.argtypes = [wt.HANDLE]; kernel.CloseHandle(h); raise
    with os.fdopen(fd, 'r', encoding='utf-8-sig') as f:
        return f.read()

def gate(path, clock=None):
    state = json.loads(read(path))
    age = (time.time() if clock is None else clock) - state['unix']
    if not state.get('ok') or state.get('pending') or not 0 <= age <= 2:
        raise RuntimeError('supervisor stopped, pending owner activity or heartbeat stale')
    return state

def progress(text, session, previous=-1, clock=None):
    lines = text.splitlines(keepends=True)
    rows = [json.loads(line) for line in lines if line.endswith('\n')]
    if not rows:
        return None
    for a, b in zip(rows, rows[1:]):
        if b['sequence'] != a['sequence'] + 1 or b['frames'] < a['frames'] or b['presents'] < a['presents']:
            raise RuntimeError('non-monotonic game readback')
    row = rows[-1]
    age = (time.time() if clock is None else clock) - row['unixMs'] / 1000
    if row['session'] != session or row['sequence'] < previous or not 0 <= age <= 3:
        raise RuntimeError('wrong session, replay or stale game readback')
    if not row['neutralInput'] or not row['forceMuted'] or row['deviceInput'] or row['network']:
        raise RuntimeError('game isolation witness refused')
    return row
