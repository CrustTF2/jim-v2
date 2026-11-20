#!/bin/sh
set -eu
cd "$(dirname "$0")"
supported_python() {
  command -v "$1" >/dev/null 2>&1 &&
    "$1" -c 'import sys; sys.exit(0 if (3,10) <= sys.version_info[:2] <= (3,12) else 1)' >/dev/null 2>&1
}

find_python() {
  for candidate in python3 python3.12 python3.11 python3.10; do
    if supported_python "$candidate"; then printf '%s\n' "$candidate"; return 0; fi
  done
  return 1
}

bundled_python() {
  runtime=runtime/python
  archive=runtime/python-3.12.tar.gz
  if [ ! -x "$runtime/bin/python3" ] && [ -f "$archive" ]; then
    if ! command -v sha256sum >/dev/null 2>&1; then
      echo 'sha256sum is required to verify the bundled Python runtime.' >&2
      return 1
    fi
    echo 'Preparing the bundled Python runtime.' >&2
    printf '%s  %s\n' '72748da13197c1fb161e3afeef20a6a385ff24f2165e6e2758e47008e7faba4c' "$archive" | sha256sum -c - >&2 || return 1
    tar -xzf "$archive" -C runtime || return 1
  fi
  if supported_python "$runtime/bin/python3"; then printf '%s\n' "$runtime/bin/python3"; return 0; fi
  return 1
}

admin() {
  if [ "$(id -u)" -eq 0 ]; then "$@"
  elif command -v sudo >/dev/null 2>&1; then sudo "$@"
  else echo 'Administrator access is needed to install Python. Install sudo or ask your administrator.' >&2; return 1
  fi
}

install_python() {
  if ! command -v apt-get >/dev/null 2>&1 || ! command -v apt-cache >/dev/null 2>&1; then
    echo 'Please install Python 3.10, 3.11, or 3.12 and its venv package using your distribution package manager.' >&2
    return 1
  fi
  echo 'Python 3.10–3.12 is missing. Installing a supported version with your system package manager.'
  echo 'Your administrator password may be requested.'
  admin apt-get update
  for version in 3.12 3.11 3.10; do
    if apt-cache show "python$version" >/dev/null 2>&1 && apt-cache show "python$version-venv" >/dev/null 2>&1; then
      admin apt-get install -y "python$version" "python$version-venv"
      return 0
    fi
  done
  echo 'No supported Python package was found in the configured repositories. Install Python 3.10–3.12 and its venv package, then retry.' >&2
  return 1
}

if [ -x .venv/bin/python3 ] && supported_python .venv/bin/python3; then
  python=.venv/bin/python3
else
  if [ -f runtime/python-3.12.tar.gz ] || [ -x runtime/python/bin/python3 ]; then
    python=$(bundled_python) || { echo 'Could not prepare the bundled Python runtime.' >&2; exit 1; }
  elif ! python=$(find_python); then
    install_python
    python=$(find_python) || { echo 'Python was installed but no supported interpreter was found.' >&2; exit 1; }
  fi
  echo "Preparing the renderer with $python."
  if ! "$python" -m venv .venv; then
    if ! command -v apt-get >/dev/null 2>&1; then
      echo 'Could not create a virtual environment. Install the venv package for your Python version and retry.' >&2
      exit 1
    fi
    version=$("$python" -c 'import sys; print("%d.%d" % sys.version_info[:2])')
    echo "Installing the Python $version virtual environment package. Your administrator password may be requested."
    admin apt-get update
    admin apt-get install -y "python$version-venv"
    "$python" -m venv .venv
  fi
  python=.venv/bin/python3
fi
"$python" -m pip install -r thumbnail_renderer/requirements.txt
echo 'War Paint preview setup is complete.'
