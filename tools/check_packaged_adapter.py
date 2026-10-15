"""Ensure patcher distributions carry the clean adapter, the open dispatcher and its notices."""
import pathlib
import sys
import zipfile

root = pathlib.Path(__file__).resolve().parents[1]
resources = root / 'overportcli/src/main/resources'
names = [
    'galaxyxr/arm64-v8a/libopenxr_loader_generic.bin',
    'galaxyxr/arm64-v8a/libopenxr_loader.bin',
    'galaxyxr/LICENSE-openxr-dispatcher-open.txt',
    'galaxyxr/LICENSE-cJSON.txt',
]
expected = {name: (resources / name).read_bytes() for name in names}
adapter = expected[names[0]]
assert b'INPUT boolean' not in adapter and b'ovrp_GetControllerState4' not in adapter
dispatcher = expected[names[1]]
assert dispatcher[:4] == b'\x7fELF' and dispatcher[18] == 0xb7, 'dispatcher is not an ARM64 ELF'
assert b'open dispatcher ' in dispatcher and b'OpenXRDispatch' in dispatcher and b'\x001.0.0\x00' in dispatcher
assert b'Copyright (c) 2026 Molier' in expected[names[2]] and b'cJSON' in expected[names[3]]
for filename in sys.argv[1:]:
    path = pathlib.Path(filename)
    candidates = list(path.rglob('*.jar')) if path.is_dir() else [path]
    found = set()
    for candidate in candidates:
        with zipfile.ZipFile(candidate) as archive:
            for name, data in expected.items():
                if name in archive.namelist():
                    assert archive.read(name) == data, (candidate, name)
                    found.add(name)
    assert found == set(expected), (filename, 'missing resources', set(expected) - found)
    print(f'{filename}: release adapter, open dispatcher 1.0.0 and licence notices present, diagnostics absent')
