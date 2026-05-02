## GNOME Rounded Blur

A standalone library providing `Blur.BlurEffect` with corner radius support for GNOME Shell extensions. It also exposes
`Blur.LiquidGlassEffect`, a separate blur effect with refraction, saturation, tint, and highlight controls for
liquid-glass style surfaces. Basically it's
just copy of [ShellBlurEffect](https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/main/src/shell-blur-effect.c) with
corner mask and different gir namespace (`Blur`).

## Build & install 

### Fedora

```bash
sudo dnf install mutter-devel
```

### From source

```bash
cd gnome-rounded-blur
meson setup build
meson compile -C build
sudo meson install -C build
```

### Arch

```bash
yay -S gnome-rounded-blur
```

### Usage in GNOME Shell Extensions

After installation, you can use the library in your extension:

```javascript
import GObject from 'gi://GObject';
import Blur from 'gi://Blur';

const MyBlurEffect = GObject.registerClass({
    GTypeName: "MyBlurEffect"
}, class MyBlurEffect extends Blur.BlurEffect {
    constructor(params) {
        super({
            mode: Blur.BlurMode.BACKGROUND,
            radius: 30,
            brightness: 0.6,
            corner_radius: 14,  // From here you can set corner radius for blur
            ...params
        });
    }
});
```

For a liquid glass surface, call the separate effect class:

```javascript
import GObject from 'gi://GObject';
import Blur from 'gi://Blur';

const MyLiquidGlassEffect = GObject.registerClass({
    GTypeName: "MyLiquidGlassEffect"
}, class MyLiquidGlassEffect extends Blur.LiquidGlassEffect {
    constructor(params) {
        super({
            mode: Blur.BlurMode.BACKGROUND,
            radius: 36,
            brightness: 0.72,
            corner_radius: 18,
            saturation: 1.45,
            tint: 0.2,
            highlight: 0.75,
            refraction: 28,
            ...params
        });
    }
});
```
