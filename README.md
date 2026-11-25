# oos-rpi-lcd35

The [oos-acemagic-s1](https://github.com/IO-Nji/oos-acemagic-s1) system dashboard for a
**Raspberry Pi with a 3.5" 480x320 SPI touch display** (ILI9486/ILI9488 panel, XPT2046
touch controller). Same six pages, same look, drawn at 150 % with a touch bar underneath.

![The six dashboard pages](docs/screenshots/overview.png)

*Screenshots are rendered by the bundled preview tool from sample data.*

> **Status.** Runs on a Raspberry Pi 3 A+ with a Waveshare 3.5" (B) v2 display
> (`fb_ili9486` framebuffer, `ADS7846` touch) on Raspberry Pi OS 12. Display output,
> navigation and the console hand-off were verified on that device by reading the
> framebuffer back and injecting touch events; other 480x320 SPI displays driven by
> the kernel should work but have not been tried.

## What it shows

System, Network (with Wi-Fi signal), Sensors, Docker, Services and Identity pages, a
health summary in the footer, and alerts including low supply voltage — all described in the
[oos-acemagic-s1 README](https://github.com/IO-Nji/oos-acemagic-s1#readme). The code
for them is shared, not copied: this repository adds only what is specific to the Pi.

On a Pi without Docker, remove `docker` from `pages` in the settings file. Power and
fan readings are not available on a Pi and show N/A.

## Touch navigation

| Control | Action |
| --- | --- |
| **PREV** / **NEXT** | previous or next page |
| swipe left / right across the page | next or previous page |
| **HOLD** | stop automatic rotation; shows **HELD** until tapped again |
| **CONSOLE** | hand the display to the text console; tap anywhere to return |

The console keeps its login session while the dashboard is showing, so you come back
to the same shell. On a Pi 3 A+ the service uses about 2 % of one core.

## Requirements

- The display must be driven by the kernel, so that it appears as a framebuffer
  (`/dev/fb1` or `/dev/fb0`) at 480x320, and the touch controller as an input device.
  On Raspberry Pi OS that is a `dtoverlay` line in `/boot/firmware/config.txt` for
  your display (for example `piscreen`, `waveshare35a` or `tft35a`), rotated to
  landscape.
- For the CONSOLE button: a text console on the display (`fbcon=map:1` on the
  kernel command line when the display is `/dev/fb1`).

## Install

```
git clone --recurse-submodules https://github.com/IO-Nji/oos-rpi-lcd35.git
cd oos-rpi-lcd35
sudo ./install.sh
```

The installer installs a compiler and CMake if needed, builds (several minutes on a
small Pi), installs `/usr/local/bin/oos-rpi-lcd35`, the systemd unit and
`/etc/oos-rpi-lcd35/config.conf`, then starts the service. Stop any other program
that draws on the display first.

Then calibrate the touch panel once:

```
sudo systemctl stop oos-rpi-lcd35
sudo oos-rpi-lcd35 --calibrate      # tap the three targets
```

Paste the lines it prints into `/etc/oos-rpi-lcd35/config.conf` and start the service.

## Configure

`/etc/oos-rpi-lcd35/config.conf` has the settings shared with the S1 version
(`page_interval`, `pages`, `services`, `disk`) plus:

| Setting | Meaning | Default |
| --- | --- | --- |
| `display` | framebuffer device | first 480x320 framebuffer |
| `touch` | input device | first touchscreen found |
| `console_tty` | terminal that shares the display | `/dev/tty1` |
| `console_hook` | command run when the console takes over, e.g. to set its font | none |
| `touch_swap_xy`, `touch_min_x`, `touch_max_x`, `touch_min_y`, `touch_max_y` | calibration, from `--calibrate` | driver ranges |

## How it is put together

```
core/        git submodule: oos-acemagic-s1 (drawing, fonts, theme, data providers, pages)
src/device.hpp       display size and scale, device settings
src/ui/touch.hpp     calibration and gesture recognition (no hardware)
src/ui/nav_bar.hpp   the touch bar and what each gesture does
src/hw/              framebuffer, touch input device, console terminal
tools/preview.cpp    offline renderer
tests/               unit tests
```

Pages are laid out in units of the S1's 320x170 panel. This display is 320 x 213 of
those units at 150 %: the dashboard frame takes the top 170 and the touch bar the rest.
A fix or new page in the core reaches both devices; update with
`git submodule update --remote core`.

## Development

```
cmake -S . -B build && cmake --build build
build/oos-rpi-preview --mock typical --out DIR    # render the pages to PNG
```

The build runs the unit tests and the layout and contrast checks. It needs no Pi.

## Licence

[GNU General Public License v3.0](LICENSE). Orbitron font: SIL Open Font License 1.1
(`core/third_party/Orbitron-OFL.txt`).
