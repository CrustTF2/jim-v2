#!/bin/sh
set -eu
if ! command -v apt-get >/dev/null 2>&1 || ! command -v apt-cache >/dev/null 2>&1; then
  echo 'This helper requires the APT package manager used by Ubuntu-based distributions.' >&2
  exit 1
fi
admin() {
  if [ "$(id -u)" -eq 0 ]; then "$@"
  elif command -v sudo >/dev/null 2>&1; then sudo "$@"
  else echo 'Administrator access is required to install system libraries.' >&2; return 1
  fi
}
echo 'Installing the required Linux graphics and networking libraries. Your administrator password may be requested.'
admin apt-get update
if LC_ALL=C apt-cache policy libcurl4t64 | grep -q 'Candidate: [^()]'; then
  curl_package=libcurl4t64
elif LC_ALL=C apt-cache policy libcurl4 | grep -q 'Candidate: [^()]'; then
  curl_package=libcurl4
else
  echo 'Could not find a supported libcurl package in the configured repositories.' >&2
  exit 1
fi
admin apt-get install -y libsdl2-2.0-0 libglu1-mesa libgl1 libegl1 libgl1-mesa-dri ca-certificates "$curl_package"
