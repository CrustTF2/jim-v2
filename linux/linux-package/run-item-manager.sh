#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ "$(uname -m)" != x86_64 ]; then
  echo 'This test build requires an x86-64 Linux system.' >&2
  exit 1
fi
if [ ! -x .venv/bin/python3 ]; then
  echo 'First run: bash setup-renderer.sh' >&2
  exit 1
fi
missing=$(ldd ./item-manager | grep 'not found' || true)
if [ -n "$missing" ]; then
  printf 'Missing Linux libraries:\n%s\n' "$missing" >&2
  echo 'On Ubuntu-based systems, run: bash install-system-dependencies.sh' >&2
  exit 1
fi
export PATH="$(pwd)/.venv/bin:$PATH"
exec ./item-manager
