"""Fail before building if the distribution, tag and release notes disagree."""
import os
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
props = dict(line.split('=', 1) for line in (root / 'galaxyxr.properties').read_text().splitlines() if '=' in line)
version = props['version']
assert re.fullmatch(r'\d+\.\d+\.\d+', version), version
assert int(props['versionCode']) > 0
source = (root / 'overportcli/src/main/java/moe/crx/overport/versions/VersionManager.kt').read_text()
assert f'const val DISTRIBUTION_VERSION = "{version}"' in source
assert (root / f'docs/RELEASE-{version}.md').is_file()
ref = os.getenv('GITHUB_REF', '')
if ref.startswith('refs/tags/'):
    assert ref == f'refs/tags/galaxyxr-v{version}', (ref, version)
print(f'Galaxy XR {version}, Android versionCode {props["versionCode"]}: consistent')
