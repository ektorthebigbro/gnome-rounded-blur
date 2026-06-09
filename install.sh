#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build and install gnome-rounded-blur using Meson.
# Works on Fedora, Arch, Debian/Ubuntu, openSUSE, and similar distributions.
set -euo pipefail

original_args=("$@")

usage() {
    cat <<'EOF'
Usage: install.sh [OPTIONS]

Build and install gnome-rounded-blur using Meson.
Build dependencies are installed automatically via the system package manager.

Options:
  --prefix PREFIX      Installation prefix (default: /usr)
  --libdir DIR         Library subdirectory relative to prefix
                       (default: auto-detected via pkg-config or filesystem)
  --build-dir NAME     Meson build directory name (default: build)
  --also-local         Also install under /usr/local using build-local/
                       (installs to both /usr and /usr/local — development workflow)
  --local              Install under /usr/local only (prefix defaults to /usr/local)
  --skip-deps          Skip automatic dependency installation
  --no-sudo            Run meson install without sudo (deps still use sudo)
  --no-ldconfig        Skip ldconfig after install
  -h, --help           Show this help

Examples:
  # Typical system install (Fedora/Arch/Debian/openSUSE):
  sudo ./install.sh

  # Install to both /usr and /usr/local for local development testing:
  sudo ./install.sh --also-local

  # Install to /usr/local only:
  sudo ./install.sh --local

  # Explicit prefix and libdir (e.g. Fedora multilib):
  sudo ./install.sh --prefix /usr --libdir lib64

  # Custom build directory:
  sudo ./install.sh --build-dir mybuild

  # Skip dep install (deps already present):
  sudo ./install.sh --skip-deps

  # Install without sudo (e.g. DESTDIR workflow or fakeroot):
  ./install.sh --no-sudo
EOF
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ ! -f "${repo_root}/meson.build" || ! -d "${repo_root}/src" ]]; then
    if ! command -v curl >/dev/null 2>&1; then
        printf 'error: curl is required when install.sh is run outside a source checkout\n' >&2
        exit 1
    fi
    if ! command -v tar >/dev/null 2>&1; then
        printf 'error: tar is required when install.sh is run outside a source checkout\n' >&2
        exit 1
    fi

    tmp_bootstrap="$(mktemp -d)"
    trap 'rm -rf "${tmp_bootstrap}"' EXIT
    tag="$(
        curl -fsSL "https://api.github.com/repos/ektorthebigbro/gnome-rounded-blur/releases/latest" |
            sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' |
            head -n1
    )"
    [[ -n "${tag}" ]] || { printf 'error: could not resolve latest gnome-rounded-blur release tag\n' >&2; exit 1; }
    curl -fL --retry 3 --connect-timeout 15 \
        "https://github.com/ektorthebigbro/gnome-rounded-blur/archive/refs/tags/${tag}.tar.gz" |
        tar -xz -C "${tmp_bootstrap}" --strip-components=1
    exec bash "${tmp_bootstrap}/install.sh" "${original_args[@]}"
fi

# Defaults
prefix="/usr"
libdir=""
build_dir="build"
also_local=0
local_only=0
skip_deps=0
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
        --skip-deps)
            skip_deps=1
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

# ---------------------------------------------------------------------------
# Dependency installation
# ---------------------------------------------------------------------------

# Find the highest-versioned package matching a pattern in apt-cache.
apt_find_versioned_pkg() {
    apt-cache search --names-only "$1" 2>/dev/null \
        | awk '{print $1}' \
        | sort -t- -k2 -rn \
        | head -1
}

install_deps_fedora() {
    printf '==> Installing build dependencies (dnf)\n'
    sudo dnf install -y gcc meson ninja-build pkgconf-pkg-config \
        glib2-devel gobject-introspection-devel mutter-devel
}

install_deps_arch() {
    printf '==> Installing build dependencies (pacman)\n'
    sudo pacman -S --needed --noconfirm \
        base-devel meson ninja gobject-introspection mutter
}

install_deps_debian() {
    printf '==> Installing build dependencies (apt)\n'
    local mutter_pkg
    mutter_pkg="$(apt_find_versioned_pkg 'libmutter-[0-9]+-dev')"
    [[ -n "${mutter_pkg}" ]] || mutter_pkg="libmutter-dev"
    sudo apt-get install -y build-essential meson ninja-build pkg-config \
        libglib2.0-dev libgirepository1.0-dev "${mutter_pkg}"
}

install_deps_suse() {
    printf '==> Installing build dependencies (zypper)\n'
    sudo zypper install -y gcc meson ninja pkgconf \
        glib2-devel gobject-introspection-devel mutter-devel
}

install_deps() {
    if [[ ! -f /etc/os-release ]]; then
        printf 'warning: /etc/os-release not found; skipping automatic dep install\n' >&2
        return
    fi
    local id id_like
    id="$(. /etc/os-release && printf '%s' "${ID:-}")"
    id_like="$(. /etc/os-release && printf '%s' "${ID_LIKE:-}")"

    case "${id}" in
        fedora|rhel|centos|almalinux|rocky|nobara)
            install_deps_fedora ;;
        arch|manjaro|endeavouros|garuda)
            install_deps_arch ;;
        debian|ubuntu|linuxmint|pop)
            install_deps_debian ;;
        opensuse*|sles)
            install_deps_suse ;;
        *)
            # Fallback: check ID_LIKE
            case "${id_like}" in
                *fedora*|*rhel*)   install_deps_fedora ;;
                *arch*)            install_deps_arch ;;
                *debian*|*ubuntu*) install_deps_debian ;;
                *suse*)            install_deps_suse ;;
                *)
                    printf 'warning: unrecognised distro "%s"; skipping automatic dep install\n' \
                        "${id}" >&2
                    printf '         Install: gcc meson ninja pkg-config glib2-devel gobject-introspection-devel mutter-devel\n' >&2
                    ;;
            esac
            ;;
    esac
}

# Skip dep install when --skip-deps was passed or when running without sudo
# (--no-sudo implies the caller has an unusual environment already set up).
if [[ "${skip_deps}" == 0 && "${use_sudo}" == 1 ]]; then
    install_deps
fi

for cmd in meson ninja pkg-config; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
        printf 'error: missing required command after dep install: %s\n' "${cmd}" >&2
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
