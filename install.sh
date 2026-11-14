#!/bin/sh
set -eu
system=$(uname -s)
case "$system" in Darwin) system=darwin ;; Linux) system=linux ;; *) echo 'Use install.ps1 on Windows' >&2; exit 1 ;; esac
machine=$(uname -m)
case "$machine" in x86_64|amd64) machine=x86_64 ;; arm64|aarch64) machine=aarch64 ;; *) echo 'Unsupported CPU architecture' >&2; exit 1 ;; esac
key="$system-$machine"
case "$key" in
  darwin-x86_64) py_url='https://github.com/astral-sh/python-build-standalone/releases/download/20261110/cpython-3.12.16%2B20261110-x86_64-apple-darwin-install_only_stripped.tar.gz'; py_sha='b78a4c479f1964ec484880bee567568266f68da102269fb93a05fe8716b45f91' ;;
  darwin-aarch64) py_url='https://github.com/astral-sh/python-build-standalone/releases/download/20261110/cpython-3.12.16%2B20261110-aarch64-apple-darwin-install_only_stripped.tar.gz'; py_sha='2f11b8aa3726bdbd87dd9eb76568412faddf4f47e26913fa45d6a2e424972acd' ;;
  linux-x86_64) py_url='https://github.com/astral-sh/python-build-standalone/releases/download/20261110/cpython-3.12.16%2B20261110-x86_64-unknown-linux-gnu-install_only_stripped.tar.gz'; py_sha='6a9e62dad033790aef722dbd370e0ce76ce17b73627ba3ae679f537049350b39' ;;
  linux-aarch64) py_url='https://github.com/astral-sh/python-build-standalone/releases/download/20261110/cpython-3.12.16%2B20261110-aarch64-unknown-linux-gnu-install_only_stripped.tar.gz'; py_sha='b6a41defec4975d94b4ff1950236e7383e5c02ba40ea48c9ec33c66087a35aab' ;;
  *) echo 'Unsupported system' >&2; exit 1 ;;
esac
cache=${BT_SOUND_CACHE:-"$HOME/.bt-sound"}
mkdir -p "$cache"
cache=$(CDPATH= cd -- "$cache" && pwd)
tmp=$(mktemp -d "$cache/bootstrap.XXXXXX")
cleanup() { case "$tmp" in "$cache"/bootstrap.*) rm -rf -- "$tmp" ;; *) echo 'Invalid temporary path' >&2 ;; esac; }
trap cleanup EXIT HUP INT TERM
verify() {
  if command -v sha256sum >/dev/null 2>&1; then actual=$(sha256sum "$1" | cut -d ' ' -f 1)
  else actual=$(shasum -a 256 "$1" | cut -d ' ' -f 1); fi
  [ "$actual" = "$2" ] || { echo 'Download SHA-256 mismatch' >&2; exit 1; }
}
python_root="$cache/python-20261110-$key"
python="$python_root/python/bin/python3"
if [ ! -x "$python" ]; then
  [ ! -e "$python_root" ] || { echo 'Incomplete Python cache; review the cache directory' >&2; exit 1; }
  echo 'Preparing portable Python (first launch only)...'
  curl -fsSL "$py_url" -o "$tmp/python.tar.gz"
  verify "$tmp/python.tar.gz" "$py_sha"
  mkdir "$tmp/runtime"
  tar -xzf "$tmp/python.tar.gz" -C "$tmp/runtime"
  mv "$tmp/runtime" "$python_root"
fi
curl -fsSL 'https://github.com/JheraldC/Nintendo-Switch-BT-Sound/releases/download/v1.2.0/bootstrap-v1.2.0.py' -o "$tmp/bootstrap.py"
verify "$tmp/bootstrap.py" 'baa57ec2c9a9f282f1e05e8019aa8c5cf9441ade1bc1b173248e23d927912df5'
if [ "${1:-}" = '--smoke-test' ] || [ -t 0 ]; then "$python" "$tmp/bootstrap.py" "$@"
elif ( : </dev/tty ) 2>/dev/null; then "$python" "$tmp/bootstrap.py" "$@" </dev/tty
else echo 'Open an interactive terminal to run the installer' >&2; exit 1; fi
