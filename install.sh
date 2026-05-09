#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build and install gnome-rounded-blur using Meson.
# Works on Fedora, Arch, Debian/Ubuntu, openSUSE, and similar distributions.
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: install.sh [OPTIONS]

Build and install gnome-rounded-blur using Meson.

Options:
  --prefix PREFIX      Installation prefix (default: /usr)
  --libdir DIR         Library subdirectory relative to prefix
                       (default: auto-detected via pkg-config or filesystem)
  --build-dir NAME     Meson build directory name (default: build)
  --also-local         Also install under /usr/local using build-local/
                       (installs to both /usr and /usr/local — development workflow)
  --local              Install under /usr/local only (prefix defaults to /usr/local)
  --no-sudo            Run meson install without sudo
  --no-ldconfig        Skip ldconfig after install
  -h, --help           Show this help

Examples:
  # Typical system install (Fedora/Arch/Debian):
  sudo ./install.sh

  # Install to both /usr and /usr/local for local development testing:
  sudo ./install.sh --also-local

  # Install to /usr/local only:
  sudo ./install.sh --local

  # Explicit prefix and libdir (e.g. Fedora multilib):
  sudo ./install.sh --prefix /usr --libdir lib64

  # Custom build directory:
  sudo ./install.sh --build-dir mybuild

  # Install without sudo (e.g. DESTDIR workflow or fakeroot):
  ./install.sh --no-sudo
EOF
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Defaults
prefix="/usr"
libdir=""
build_dir="build"
also_local=0
local_only=0
use_sudo=1
run_ldconfig=1

while (($#)); do
    case "$1" in
        --prefix)
            [[ $# -ge 2 ]] || { printf 'error: --prefix requires a value\n' >&2; exit 2; }
            prefix="$2"
            shift
            ;;
        --libdir)
            [[ $# -ge 2 ]] || { printf 'error: --libdir requires a value\n' >&2; exit 2; }
            libdir="$2"
            shift
            ;;
        --build-dir)
            [[ $# -ge 2 ]] || { printf 'error: --build-dir requires a value\n' >&2; exit 2; }
            build_dir="$2"
            shift
            ;;
        --also-local)
            also_local=1
            ;;
        --local)
            local_only=1
            ;;
        --no-sudo)
            use_sudo=0
            ;;
        --no-ldconfig)
            run_ldconfig=0
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

if [[ "${local_only}" == 1 && "${also_local}" == 1 ]]; then
    printf 'error: --local and --also-local are mutually exclusive\n' >&2
    exit 2
fi

if [[ "${local_only}" == 1 ]]; then
    prefix="/usr/local"
fi

for cmd in meson ninja pkg-config; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
        printf 'error: missing required command: %s\n' "${cmd}" >&2
        exit 1
    fi
done

if [[ "${use_sudo}" == 1 ]]; then
    if ! command -v sudo >/dev/null 2>&1; then
        printf 'error: sudo not found; use --no-sudo to install without it\n' >&2
        exit 1
    fi
    if [[ -t 0 ]]; then
        sudo -v
    else
        sudo -S -v
    fi
fi

# Detect the appropriate lib subdirectory for a given prefix.
# Uses pkg-config to query the native library path of a well-known package
# (glib-2.0) so we automatically respect multilib layouts (e.g. lib64 on
# Fedora) without requiring the caller to pass --libdir explicitly.
detect_libdir() {
    local pfx="$1"
    local sys_libdir

    if sys_libdir="$(pkg-config --variable=libdir glib-2.0 2>/dev/null)" &&
            [[ -n "${sys_libdir}" && "${sys_libdir}" == "${pfx}/"* ]]; then
        printf '%s' "${sys_libdir#"${pfx}/"}"
        return
    fi

    # Filesystem fallback: prefer lib64 only when lib does not exist.
    if [[ -d "${pfx}/lib64" && ! -d "${pfx}/lib" ]]; then
        printf 'lib64'
    else
        printf 'lib'
    fi
}

build_and_install() {
    local bdir="$1"
    local pfx="$2"
    local ldir="${3:-}"
    local full_bdir="${repo_root}/${bdir}"

    if [[ -z "${ldir}" ]]; then
        ldir="$(detect_libdir "${pfx}")"
    fi

    printf '\n==> Building: prefix=%s  libdir=%s  builddir=%s\n' \
        "${pfx}" "${ldir}" "${bdir}"

    if [[ -d "${full_bdir}" ]]; then
        meson setup "${full_bdir}" "${repo_root}" \
            --reconfigure \
            --prefix="${pfx}" \
            --libdir="${ldir}"
    else
        meson setup "${full_bdir}" "${repo_root}" \
            --prefix="${pfx}" \
            --libdir="${ldir}"
    fi

    meson compile -C "${full_bdir}"

    if [[ "${use_sudo}" == 1 ]]; then
        sudo meson install -C "${full_bdir}"
    else
        meson install -C "${full_bdir}"
    fi
}

if [[ "${also_local}" == 1 ]]; then
    build_and_install "${build_dir}" "${prefix}" "${libdir}"
    build_and_install "build-local" "/usr/local" ""
else
    build_and_install "${build_dir}" "${prefix}" "${libdir}"
fi

if [[ "${run_ldconfig}" == 1 ]] && command -v ldconfig >/dev/null 2>&1; then
    if [[ "${use_sudo}" == 1 ]]; then
        sudo ldconfig
    else
        ldconfig
    fi
fi

if pkg-config --exists blur-effect-1.0 2>/dev/null; then
    printf '\nInstalled blur-effect %s\n' "$(pkg-config --modversion blur-effect-1.0)"
else
    printf '\nwarning: blur-effect-1.0 not found in pkg-config search path\n' >&2
    if [[ "${local_only}" == 1 || "${also_local}" == 1 ]]; then
        printf '  If PKG_CONFIG_PATH does not include /usr/local/lib/pkgconfig, run:\n' >&2
        printf '    export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH\n' >&2
    fi
fi
