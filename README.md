# Reno119

Reno119 is a Linux-native Qt 6/QML manager focused on making **RenoDX HDR setup as painless as possible** for Windows games running through Proton and Wine.

It discovers games from Steam, Heroic, Lutris, and custom entries, checks what each title needs, and brings RenoDX, ReShade, REFramework, OptiScaler integration, backups, diagnostics, and recommended setup into one interface.

> Reno119 is an independent project and is not affiliated with RenoDX, ReShade, OptiScaler, REFramework, Valve, Heroic, Lutris, or PCGamingWiki.

## Features

- **Recommended Setup** shows what the selected game needs and exposes direct Install, Update, Review, Preview, Take over, and Fix actions where appropriate.
- **Set up recommended** can safely process deterministic managed actions in order while leaving ambiguous matches and external installations for review.
- **RenoDX matching safeguards** prevent unsafe title matches and require confirmation for partial matches.
- Integrated management for **ReShade, RenoDX, REFramework, and OptiScaler**.
- Library discovery for **Steam, Heroic, Lutris, and custom programs**.
- **Backups, restore points, and recovery tools** for managed changes.
- **Material 3 and Noctalia v5 theming**, including live palette reloads and Noctalia Pure Black support.
- Shared metadata and payload caches to keep routine network traffic low.

## Current version

**0.23.5**

## Requirements

Reno119 currently targets Linux and requires:

- CMake 3.24+
- Ninja
- a C++20 compiler
- Qt 6.6+
- Qt modules: Core, Gui, QML, Quick, Quick Controls 2, Network, Concurrent, DBus, and SQL

### Arch / CachyOS

```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative
```

## Build and run

```bash
git clone https://github.com/bolidbake/reno119.git
cd reno119
./build.sh
```

## Install system-wide

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr

cmake --build build
sudo cmake --install build
```

This installs the Reno119 executable, desktop launcher, and bundled theme/template support files.

An Arch/CachyOS `PKGBUILD` is also included under `packaging/arch/`. It is ready to use once a matching `v0.23.5` Git tag exists.

## Noctalia v5 theming

Reno119 can follow a Noctalia-generated Material palette live.

From a Reno119 source checkout:

```bash
mkdir -p ~/.config/noctalia/templates ~/.config/reno119

install -Dm644 \
  data/noctalia/reno119-theme.json.template \
  ~/.config/noctalia/templates/reno119-theme.json.template
```

Register the template in a Noctalia TOML file such as `~/.config/noctalia/reno119.toml`:

```toml
[theme.templates.user.reno119]
input_path = "$XDG_CONFIG_HOME/noctalia/templates/reno119-theme.json.template"
output_path = "$XDG_CONFIG_HOME/reno119/noctalia-theme.json"
```

Render the template:

```bash
noctalia msg templates-apply
```

Then select **Noctalia** in Reno119's Appearance settings. Reno119 watches the generated palette file and reloads theme changes live.

## Project scope

Reno119 is intentionally not a general-purpose game launcher or mod manager. Its main goal is a straightforward workflow:

**select game → see recommended setup → install/apply → get HDR working**

Advanced component controls remain available for troubleshooting and manual management.

## Documentation

- Changelog: [`CHANGELOG.md`](CHANGELOG.md)
- Project site source: [`docs/`](docs/)
- Contributing: [`CONTRIBUTING.md`](CONTRIBUTING.md)

## License

Reno119 is released under **GPL-3.0-or-later**.
