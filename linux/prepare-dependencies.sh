#!/bin/sh
set -eu
cd "$(dirname "$0")"
python3 - <<'PY'
import hashlib, json, pathlib, tarfile, tempfile, urllib.request

root=pathlib.Path.cwd()
dependencies=json.loads((root/'dependency-sources.json').read_text(encoding='utf-8'))
destination=root/'deps'
destination.mkdir(exist_ok=True)
for entry in dependencies:
    if entry['name'] not in ('protobuf','jsoncpp'):
        continue
    target=destination/entry['name']
    if target.exists():
        print('Using existing dependency:',entry['name'])
        continue
    print('Downloading',entry['name'],flush=True)
    with tempfile.TemporaryDirectory(dir=destination) as tmp:
        tmp=pathlib.Path(tmp)
        archive=tmp/'source.tar.gz'
        with urllib.request.urlopen(entry['url'],timeout=120) as response, archive.open('wb') as out:
            while True:
                block=response.read(1024*1024)
                if not block:break
                out.write(block)
        digest=hashlib.sha256(archive.read_bytes()).hexdigest()
        if digest!=entry['sha256']:
            raise SystemExit('Checksum mismatch: '+entry['name'])
        with tarfile.open(archive,'r:gz') as source:
            for member in source.getmembers():
                parts=pathlib.PurePosixPath(member.name).parts
                if not parts or member.name.startswith('/') or parts[0] in ('.','..') or '..' in parts or not (member.isfile() or member.isdir()):
                    raise SystemExit('Unsafe archive entry: '+member.name)
            source.extractall(tmp/'unpacked')
        extracted=list((tmp/'unpacked').iterdir())
        if len(extracted)!=1 or not extracted[0].is_dir():
            raise SystemExit('Unexpected archive layout: '+entry['name'])
        extracted[0].rename(target)
print('Pinned Linux build dependencies are ready.')
PY
