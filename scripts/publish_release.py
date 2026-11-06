"""Create a release, or verify an already-published release without replacing it."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
from urllib.parse import quote
import zipfile

PACKAGES = ('openDucker-macOS-universal.zip', 'openDucker-Windows-x64.zip')
ASSETS = (*PACKAGES, 'SHA256SUMS.txt')


def validate_packages(directory, repository, revision):
    checksums = {}
    for line in (directory / 'SHA256SUMS.txt').read_text().splitlines():
        match = re.fullmatch(r'([a-fA-F0-9]{64}) [ *](.+)', line)
        if not match or match[2] not in PACKAGES or match[2] in checksums:
            raise ValueError('Invalid or duplicate entry in SHA256SUMS.txt')
        checksums[match[2]] = match[1].lower()
    if set(checksums) != set(PACKAGES):
        raise ValueError('SHA256SUMS.txt must cover both release packages')
    source = f'Plugin source and build instructions: https://github.com/{repository}/tree/{revision}'
    for name in PACKAGES:
        path = directory / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != checksums[name]:
            raise ValueError(f'Checksum mismatch: {name}')
        with zipfile.ZipFile(path) as archive:
            if archive.testzip() is not None:
                raise ValueError(f'Corrupt ZIP: {name}')
            if archive.read('package/SOURCE.txt').decode().splitlines()[0] != source:
                raise ValueError(f'Package source does not match the release tag: {name}')


def lookup_release(repository, tag):
    result = subprocess.run(
        ['gh', 'api', f'repos/{repository}/releases/tags/{quote(tag, safe="")}'],
        capture_output=True, text=True)
    try:
        release = json.loads(result.stdout or '{}')
    except json.JSONDecodeError as error:
        raise RuntimeError(f'Release lookup failed: {result.stderr.strip()}') from error
    if result.returncode:
        if str(release.get('status')) != '404':
            raise RuntimeError(f'Release lookup failed: {result.stderr.strip()}')
        return None
    return release


def verify_existing_release(release, tag, repository, revision):
    if release['tag_name'] != tag or release['draft'] or not release['published_at']:
        raise ValueError('Existing release must be published for the requested tag')
    uploaded = {asset['name'] for asset in release['assets'] if asset['state'] == 'uploaded'}
    if not set(ASSETS).issubset(uploaded):
        raise ValueError('Existing release is incomplete; refusing to replace published assets')
    with tempfile.TemporaryDirectory() as temporary:
        command = ['gh', 'release', 'download', tag, '--repo', repository, '--dir', temporary]
        for name in ASSETS:
            command.extend(['--pattern', name])
        subprocess.run(command, check=True)
        # Rebuilt ZIPs can differ; the published manifest authenticates the published bytes.
        validate_packages(Path(temporary), repository, revision)
    print(f'Existing release {tag} has valid packages for {revision}; preserved all assets.')


def publish(tag, repository, source_directory, dist):
    if not tag.startswith('v'):
        raise ValueError('Release tag must start with v')
    revision = subprocess.check_output(
        ['git', '-C', str(source_directory), 'rev-parse', '--verify', f'refs/tags/{tag}^{{commit}}'],
        text=True).strip()
    checked_out = subprocess.check_output(
        ['git', '-C', str(source_directory), 'rev-parse', 'HEAD'], text=True).strip()
    if checked_out != revision:
        raise ValueError('Checked-out source does not match the release tag')

    release = lookup_release(repository, tag)
    if release is None:
        validate_packages(dist, repository, revision)
        try:
            subprocess.run([
                'gh', 'release', 'create', tag, *(str(dist / name) for name in ASSETS),
                '--repo', repository, '--verify-tag', '--target', revision,
                '--notes-file', str(source_directory / 'docs/RELEASE_NOTES.md'),
                '--title', f'openDucker {tag}',
            ], check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as error:
            message = f'{error.stderr or ""}\n{error.stdout or ""}'
            # gh reports this when the tag's release was created between the lookup and create.
            if 'already exists' not in message:
                raise
            release = lookup_release(repository, tag)
            if release is None:
                raise
            verify_existing_release(release, tag, repository, revision)
        return

    verify_existing_release(release, tag, repository, revision)


if __name__ == '__main__':
    publish(os.environ['RELEASE_TAG'], os.environ['GITHUB_REPOSITORY'],
            Path('release-source'), Path('dist'))
