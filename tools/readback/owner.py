"""Read-only owner-state snapshot or independent exact-identity comparison."""
import json, os, pathlib, sys
from inventory import inventory

def snapshot(roots):
    result = {}
    for name in roots:
        p = pathlib.Path(name)
        dirs = {}
        if p.exists():
            for parent, names, _ in os.walk(p, followlinks=False):
                for d in [pathlib.Path(parent)] + [pathlib.Path(parent) / n for n in names]:
                    s = d.stat(follow_symlinks=False)
                    if s.st_file_attributes & 0x400: raise ValueError(f'reparse directory refused: {d}')
                    dirs[d.relative_to(p).as_posix()] = dict(attributes=s.st_file_attributes,
                        creationUtcTicks=s.st_birthtime_ns // 100 + 621355968000000000,
                        writeUtcTicks=s.st_mtime_ns // 100 + 621355968000000000)
        result[name] = {'present': p.exists(), 'directories': dirs, 'files': inventory(p) if p.exists() else {}}
    return result

if __name__ == '__main__':
    mode, rootsfile, out = sys.argv[1:4]
    roots = json.loads(pathlib.Path(rootsfile).read_text(encoding='utf-8-sig'))
    actual = snapshot(roots)
    pathlib.Path(out).write_text(json.dumps(actual, indent=1, sort_keys=True))
    if mode == 'verify':
        expected = json.loads(pathlib.Path(sys.argv[4]).read_text())
        if actual != expected:
            raise SystemExit('Owner presence/SHA256/UTC-ticks/attributes changed; retain custody')
