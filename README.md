# GNOME Rounded Blur

`gnome-rounded-blur` is a small GNOME Shell support library that exposes
blur effects through the `Blur` GObject Introspection namespace. It provides a
drop-in rounded variant of GNOME Shell's blur effect plus a Liquid Glass effect
for extensions that need refraction, depth, and edge glow controls.

This fork is maintained for Haze and other GNOME Shell extensions that need
native rounded blur surfaces on current GNOME releases.

## Features

- `Blur.BlurEffect`: native background or actor blur with configurable corner
  radius and adaptive brightness.
- `Blur.LiquidGlassEffect`: a separate glass renderer with blur, brightness,
  radius, adaptive brightness, refraction, depth, and glow tuning.
- GObject Introspection support for GJS extensions via `import Blur from
  'gi://Blur'`.
- Mutter compatibility detection for GNOME 48, 49, and 50.
- Installed runtime search paths for Mutter's private libraries.

## Compatibility

The Meson build probes the installed Mutter development packages and selects
the newest supported API it can find:

| GNOME Shell | Mutter API | Requirement |
| --- | --- | --- |
| 50 | `mutter-18` | `>= 50.0` |
| 49 | `mutter-17` | `>= 49.0` |
| 48 | `mutter-16` | `>= 48.0` |

You must build the library against the same GNOME/Mutter major version that
your GNOME Shell session is running.

## Installation

The repository ships `install.sh`, a cross-distro helper that wraps the Meson
build and install steps.  It detects your distribution, installs the required
build dependencies automatically, and then builds and installs the library.

### Quick install (any distro)

From the latest GitHub release:

```bash
curl -fsSL https://github.com/ektorthebigbro/gnome-rounded-blur/releases/latest/download/install.sh | bash
```

From a source checkout:

```bash
sudo ./install.sh
```

Supported package managers: `dnf` (Fedora/RHEL/Nobara), `pacman` (Arch/Manjaro),
`apt` (Debian/Ubuntu/Mint/Pop), `zypper` (openSUSE).

If your distro is not recognised, or you prefer to manage dependencies
yourself, install them manually then pass `--skip-deps`:

#### Fedora / RHEL / Nobara

```bash
sudo dnf install gcc meson ninja-build pkgconf-pkg-config \
    glib2-devel gobject-introspection-devel mutter-devel
sudo ./install.sh --skip-deps
```

#### Arch / Manjaro

```bash
sudo pacman -S --needed base-devel meson ninja gobject-introspection mutter
sudo ./install.sh --skip-deps
```

#### Debian / Ubuntu / Mint / Pop

Replace `<N>` with the mutter version that matches your GNOME release
(e.g. `14` for GNOME 46, `16` for GNOME 48):

```bash
sudo apt install build-essential meson ninja-build pkg-config \
    libglib2.0-dev libgirepository1.0-dev libmutter-<N>-dev
sudo ./install.sh --skip-deps
```

#### openSUSE

```bash
sudo zypper install gcc meson ninja pkgconf \
    glib2-devel gobject-introspection-devel mutter-devel
sudo ./install.sh --skip-deps
```

### Manual build (no script)

If `install.sh` does not work, build with Meson directly:

```bash
meson setup build --prefix=/usr
meson compile -C build
sudo meson install -C build
sudo ldconfig
```

On Fedora and other multilib systems replace `--prefix=/usr` with
`--prefix=/usr --libdir=lib64` if your system libraries live under `/usr/lib64`.

### install.sh flags

| Flag | Description |
| --- | --- |
| `--prefix PREFIX` | Installation prefix (default: `/usr`) |
| `--libdir DIR` | Library subdirectory relative to prefix (auto-detected by default) |
| `--build-dir NAME` | Meson build directory name (default: `build`) |
| `--also-local` | Also install under `/usr/local` using `build-local/` — installs to both `/usr` and `/usr/local` |
| `--local` | Install under `/usr/local` only |
| `--skip-deps` | Skip automatic dependency installation |
| `--no-sudo` | Run `meson install` without `sudo` (also skips dep install) |
| `--no-ldconfig` | Skip `ldconfig` after install |

### Local Development

To install to both `/usr` and `/usr/local` (matching the one-click
`push-to-system.sh` workflow used by the Haze companion extension):

```bash
sudo ./install.sh --also-local
```

Restart GNOME Shell, or log out and back in, after installing or replacing the
library.

## Usage

Import the `Blur` namespace from GJS and attach the effect to a `Clutter.Actor`
or `St.Widget`.

### Rounded Blur

```javascript
import GObject from 'gi://GObject';
import Blur from 'gi://Blur';

const MyBlurEffect = GObject.registerClass({
    GTypeName: 'MyBlurEffect',
}, class MyBlurEffect extends Blur.BlurEffect {
    constructor(params = {}) {
        super({
            mode: Blur.BlurMode.BACKGROUND,
            radius: 30,
            brightness: 0.6,
            corner_radius: 14,
            ...params,
        });
    }
});
```

### Liquid Glass

```javascript
import GObject from 'gi://GObject';
import Blur from 'gi://Blur';

const MyLiquidGlassEffect = GObject.registerClass({
    GTypeName: 'MyLiquidGlassEffect',
}, class MyLiquidGlassEffect extends Blur.LiquidGlassEffect {
    constructor(params = {}) {
        super({
            mode: Blur.BlurMode.BACKGROUND,
            radius: 44,
            brightness: 0.82,
            corner_radius: 28,
            refraction: 20,
            depth: 70,
            glow_weight: 15,
            glow_bias: 0,
            glow_bevel: 3,
            glow_smooth: 10,
            ...params,
        });
    }
});
```

## API Reference

### Shared Properties

| Property | Type | Description |
| --- | --- | --- |
| `mode` | `Blur.BlurMode` | `BACKGROUND` samples the stage behind the actor; `ACTOR` blurs the actor's own contents. |
| `radius` | integer | Blur radius in pixels. Extensions often expose this as `sigma * 2`. |
| `brightness` | float `0.0..1.0` | Multiplier applied after blur. |
| `corner_radius` | float | Rounded mask radius in logical pixels before scale-factor conversion. |
| `adaptive_brightness` | boolean | Darkens bright backdrop pixels in the native brightness pass. |
| `adaptive_brightness_strength` | float `0.0..1.0` | How aggressively bright pixels are darkened. |
| `adaptive_brightness_minimum` | float `0.0..1.0` | Lowest brightness multiplier adaptive brightness may choose. |

### Liquid Glass Properties

| Property | Type | Default | Description |
| --- | --- | --- | --- |
| `refraction` | float `0..150` | `20` | How strongly the glass bends the blurred backdrop. |
| `depth` | float `0..150` | `70` | How quickly the lens compression builds toward the center. |
| `glow_weight` | float `0..100` | `15` | Edge glow intensity. |
| `glow_bias` | float `-100..100` | `0` | Global offset applied to the glow multiplier. |
| `glow_bevel` | float `0..100` | `3` | Width of the bevel region near the edge. |
| `glow_smooth` | float `0..100` | `10` | Smoothness of the bevel fade. |
| `highlight` | float `0..1` | `0.35` | Compatibility alias that maps to `glow_weight`. Prefer `glow_weight` for new code. |

## Detecting Support

Extensions should check whether the namespace and effect class are available
before selecting a rendering path:

```javascript
const roundedBlurAvailable = Boolean(Blur?.BlurEffect);
const liquidGlassAvailable = Boolean(Blur?.LiquidGlassEffect);
let adaptiveBrightnessAvailable = false;
try {
    const effect = new Blur.BlurEffect();
    effect.set_property('adaptive-brightness', false);
    adaptiveBrightnessAvailable = true;
} catch (_) {}
```

If `Blur.LiquidGlassEffect` is missing, fall back to `Blur.BlurEffect` or
GNOME Shell's built-in `Shell.BlurEffect`.

## Troubleshooting

- `Typelib file for namespace 'Blur' not found`: the library was not installed
  into a GI search path for the running session. Reinstall and restart GNOME
  Shell.
- `libmutter-*` loading errors: the library was built against a different
  GNOME/Mutter major version than the active session. Rebuild against the
  current system packages.
- Liquid Glass looks static while windows move behind it: the extension must
  queue repaints for glass actors when nearby scene content changes.
- Corners are square: make sure you are setting `corner_radius`, not only CSS
  `border-radius`.

## License

This project is licensed under GPL-3.0-or-later. See [LICENSE](LICENSE).
