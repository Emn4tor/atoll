# Atoll

**A dynamic island for KDE Plasma.**

Atoll is a small overlay that lives at the top of your screen and morphs to
show whatever just happened: the volume you changed, the notification that
arrived, the track that started playing. Click it and it unfolds into a
dashboard with a full transport, your notification backlog and a few toggles.

It is a native Qt 6 / QML application built on `wlr-layer-shell` — no
Quickshell, no AGS, no Hyprland. It runs as a normal program on any Wayland
compositor that speaks layer-shell, and it is built to fit Plasma in
particular.

![the island at rest, with a satellite while music plays](docs/screenshots/idle.png)

## What it does

| State | What you see |
|---|---|
| **Idle** | A bare notch, or the clock. A satellite blob buds off it while music plays. |
| **OSD** | Volume, brightness, microphone, keyboard layout, power profile — anything Plasma announces. |
| **Notification** | App icon or image, summary and body, with a progress bar for the notifications that carry one. |
| **Media** | Album art, scrolling title, spectrum, and transport controls that fade in on hover. |
| **Expanded** | Clock, battery, seekable transport, notification history and quick toggles. |

### How it gets its information

Atoll never takes anything over. It does not replace your notification daemon
and it does not replace Plasma's OSD:

- **OSD events** are read by watching the method calls Plasma already sends to
  `org.kde.osdService`. That means hardware keys, third-party mixers and
  Plasma's own applets all reach the island without polling PipeWire.
- **Notifications** are read the same way, by asking the bus daemon to make
  Atoll a monitor. Your real daemon — Plasma's, dunst, whatever you run —
  keeps doing its job; Atoll mirrors it. Pairing each observed `Notify` call
  with its reply also recovers the id the daemon handed out, which is what
  makes closing a notification from the island possible.
- **Media** is plain MPRIS2.
- **The accent colour** follows the dominant colour of the current album art.

## Installing

Dependencies: `qt6-base qt6-declarative qt6-svg layer-shell-qt kcoreaddons ki18n dbus`,
plus `cmake ninja qt6-shadertools extra-cmake-modules` to build.
Optional: `cava` for a real spectrum, `wireplumber` for volume control.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

On Arch there is a `packaging/PKGBUILD`.

Start it with `atoll`, or enable the bundled user service:

```sh
systemctl --user enable --now atoll.service
```

## Controlling it

Atoll exposes `org.atoll.Atoll` on the session bus, which makes the island a
general purpose heads-up display for your own scripts:

```sh
atollctl text drive-harddisk "Backup finished"
atollctl progress cloud-upload 64 "Uploading"
atollctl toggle
```

Interaction: click to expand, middle-click to play/pause, scroll to change
volume, hover to reveal transport controls.

## Configuring

`~/.config/atoll/atoll.json` is written on first run with every default
spelled out, and it is re-read live — save the file and the island changes
while you watch. Keys you are most likely to want:

| Key | Meaning |
|---|---|
| `island.screen` | `"primary"` or an output name such as `"HDMI-A-1"`. |
| `island.idleMode` | `"notch"`, `"clock"` or `"hidden"`. |
| `island.exclusiveZone` | `0` sits below your panels, `-1` overlaps them. |
| `appearance.accent` | `"auto"` follows the album art, or pin a colour. |
| `effects.gooey` | The metaball merge between the island and its satellite. |
| `notifications.ignoredApps` | Apps the island should stay quiet about. |
| `media.preferred` | Player names to favour, in order. |

## Troubleshooting

| Variable | Effect |
|---|---|
| `ATOLL_DEBUG_STATE=1` | Logs every state change and incoming event. |
| `ATOLL_DEBUG_SURFACE=1` | Paints the whole layer surface so its bounds are visible. |
| `ATOLL_DEBUG_GRAB=<path>` | Saves one rendered frame and exits (`ATOLL_DEBUG_GRAB_DELAY` in ms). |
| `ATOLL_NO_LAYER_SHELL=1` | Falls back to an ordinary window, for X11 or a nested compositor. |

If the island says it cannot watch the session bus, the bus daemon refused
`BecomeMonitor`; media control still works but OSD and notification mirroring
do not.

## Known limitations

- Notification **actions** are best effort. As a bus observer Atoll can
  re-broadcast `ActionInvoked`, but senders that filter on the daemon's bus
  name will ignore it.
- One island, on one screen, per instance.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
