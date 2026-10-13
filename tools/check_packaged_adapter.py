"""Ensure both patcher distributions carry the clean adapter used during conversion."""
import pathlib
import sys
import zipfile

root = pathlib.Path(__file__).resolve().parents[1]
name = 'galaxyxr/arm64-v8a/libopenxr_loader_generic.bin'
expected = (root / 'overportcli/src/main/resources' / name).read_bytes()
assert b'INPUT boolean' not in expected and b'ovrp_GetControllerState4' not in expected
for filename in sys.argv[1:]:
    with zipfile.ZipFile(filename) as archive:
        assert archive.read(name) == expected, filename
    print(f'{filename}: release adapter present, diagnostics absent')
