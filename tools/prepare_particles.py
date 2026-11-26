#!/usr/bin/env python3
"""Convert supplied WebP particles to compact, independently decodable frames."""
import argparse
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path
import struct
import tempfile
import zlib

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SIZE = 96


def convert(source_and_destination):
    source, destination = map(Path, source_and_destination)
    png, animation = destination / (source.stem + '.png'), destination / (source.stem + '.jpa')
    if png.is_file() and animation.is_file() and png.stat().st_mtime >= source.stat().st_mtime and animation.stat().st_mtime >= source.stat().st_mtime:
        return source.name
    with Image.open(source) as image:
        if not 1 <= image.n_frames <= 4096:
            raise ValueError('unsupported frame count')
        frames = []
        first = None
        for index in range(image.n_frames):
            image.seek(index)
            frame = image.convert('RGBA').resize((SIZE, SIZE), Image.Resampling.LANCZOS)
            if first is None:
                first = frame.copy()
            duration = max(10, min(60000, int(image.info.get('duration', 100))))
            compressed = zlib.compress(frame.tobytes(), 6)
            frames.append((duration, compressed))
        with tempfile.NamedTemporaryFile(dir=destination, delete=False) as temp:
            temp.write(struct.pack('<4sHHI', b'JPA1', SIZE, SIZE, len(frames)))
            for duration, compressed in frames:
                temp.write(struct.pack('<II', duration, len(compressed)))
                temp.write(compressed)
            temp_path = Path(temp.name)
        temp_path.replace(animation)
        with tempfile.NamedTemporaryFile(dir=destination, suffix='.png', delete=False) as temp:
            png_temp = Path(temp.name)
        first.save(png_temp, format='PNG', optimize=True)
        png_temp.replace(png)
    return source.name


def main():
    parser = argparse.ArgumentParser()
    local_particles = ROOT / 'Particles'
    default_input = local_particles if local_particles.is_dir() else ROOT.parents[1] / 'Particles'
    parser.add_argument('--input', type=Path, default=default_input)
    parser.add_argument('--output', type=Path, default=ROOT / 'particle-assets')
    parser.add_argument('--workers', type=int, default=4)
    args = parser.parse_args()
    files = sorted(args.input.glob('*.webp'))
    if not files:
        parser.error('No .webp files found in ' + str(args.input))
    args.output.mkdir(parents=True, exist_ok=True)
    errors = []
    with ProcessPoolExecutor(max_workers=max(1, args.workers)) as pool:
        jobs = {pool.submit(convert, (str(path), str(args.output))): path for path in files}
        for count, future in enumerate(as_completed(jobs), 1):
            try:
                future.result()
            except Exception as exc:
                errors.append((jobs[future].name, str(exc)))
            if count % 25 == 0 or count == len(files):
                print(f'Prepared {count}/{len(files)} particle effects', flush=True)
    for name, error in errors:
        print(f'{name}: {error}', flush=True)
    if errors:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

