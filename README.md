iOS Push Email plugin for Dovecot
=================================

[![CI](https://github.com/timlaing/dovecot-xaps-plugin/actions/workflows/ci.yml/badge.svg)](https://github.com/timlaing/dovecot-xaps-plugin/actions/workflows/ci.yml)
[![Lint](https://github.com/timlaing/dovecot-xaps-plugin/actions/workflows/lint.yml/badge.svg)](https://github.com/timlaing/dovecot-xaps-plugin/actions/workflows/lint.yml)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=timlaing_dovecot-xaps-plugin&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=timlaing_dovecot-xaps-plugin)
[![Release](https://img.shields.io/github/v/release/timlaing/dovecot-xaps-plugin)](https://github.com/timlaing/dovecot-xaps-plugin/releases)
[![License](https://img.shields.io/github/license/timlaing/dovecot-xaps-plugin)](LICENSE)

This project provides Dovecot 2.4.x and Dovecot 2.3.x plugins for the `XAPPLEPUSHSERVICE` IMAP extension and
mail-event notifications. It works with [dovecot-xaps-daemon](https://github.com/timlaing/dovecot-xaps-daemon) to
deliver native Apple Push Notification Service (APNS) notifications for iOS Mail.

Both components are required:

1. This repository supplies the Dovecot IMAP and push-notification plugins.
2. The daemon receives registrations and mail events from Dovecot and sends notifications to APNS.

Apple did not publish an XAPPLEPUSHSERVICE specification. The implementation was derived from Apple's published
Dovecot patches and requires APNS credentials that you are legally entitled to use.

Installation
============

Prerequisites
-------------

* A working Dovecot installation. Releases provide packages for both the Dovecot 2.4.x and 2.3.x series.
* Mail delivery through Dovecot LDA or LMTP.
* A running [dovecot-xaps-daemon](https://github.com/timlaing/dovecot-xaps-daemon).

APT repository
--------------

For Ubuntu and Debian systems, add the signed APT repository for the latest packages:

```sh
# Add the repository GPG key
sudo curl -fsSL https://raw.githubusercontent.com/timlaing/dovecot-xaps-apt/main/public-key.asc -o /usr/share/keyrings/dovecot-xaps-archive-keyring.asc

# Add the repository (pick the suite matching your Dovecot version)
# Dovecot 2.4 (Ubuntu 26.04 and newer):
echo "deb [signed-by=/usr/share/keyrings/dovecot-xaps-archive-keyring.asc] https://timlaing.github.io/dovecot-xaps-apt/ resolute main" | sudo tee /etc/apt/sources.list.d/dovecot-xaps.list
# Dovecot 2.3 (Ubuntu 24.04) instead:
# echo "deb [signed-by=/usr/share/keyrings/dovecot-xaps-archive-keyring.asc] https://timlaing.github.io/dovecot-xaps-apt/ noble main" | sudo tee /etc/apt/sources.list.d/dovecot-xaps.list

# Update and install
sudo apt-get update
sudo apt-get install dovecot-xaps-plugin
```

See [timlaing/dovecot-xaps-apt](https://github.com/timlaing/dovecot-xaps-apt) for more information.

Debian package
--------------

Tagged releases include a Debian package built against Dovecot 2.4 (Ubuntu 26.04, `...-1`) and against Dovecot 2.3
(Ubuntu 24.04, `...-1~dov23`). Download the package matching your Dovecot version from this repository's
[Releases](https://github.com/timlaing/dovecot-xaps-plugin/releases) page and install it with APT:

```sh
sudo apt install ./dovecot-xaps-plugin_<version>_<architecture>.deb
```

The Dovecot 2.4 package installs:

* Mail plugins in `/usr/lib/dovecot/modules/`.
* The settings module in `/usr/lib/dovecot/modules/settings/`.
* Dovecot configuration in `/etc/dovecot/conf.d/95-xaps.conf`.

The Dovecot 2.3 package installs the same files except the settings module (Dovecot 2.3 reads plugin settings from the
`plugin { }` block instead). The two packages cannot be installed side by side because their `dovecot-core` major
versions conflict.

The default daemon endpoint is the IPv6 loopback listener used by xapsd:

```dovecot
xaps_url = http://[::1]:11619
```

Change `xaps_url` if the daemon listens at a different address. Validate and restart Dovecot after installation:

```sh
sudo doveconf -n
sudo systemctl restart dovecot
```

Build from source
-----------------

On Ubuntu 26.04 LTS (Dovecot 2.4, default) or Ubuntu 24.04 LTS (Dovecot 2.3):

```sh
sudo apt-get update
sudo apt-get install build-essential git dovecot-dev cmake
git clone https://github.com/timlaing/dovecot-xaps-plugin.git
cd dovecot-xaps-plugin
# Omit -DXAPS_DOVECOT_MAJOR to build for Dovecot 2.4 (default).
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
  -DXAPS_DOVECOT_MAJOR=2.3
cmake --build build --parallel
sudo cmake --install build
sudo doveconf -n
sudo systemctl restart dovecot
```

`-DXAPS_DOVECOT_MAJOR=2.4` builds the Dovecot 2.4 modules (`lib25_` prefix, settings module, config-section based
push-notification driver). `-DXAPS_DOVECOT_MAJOR=2.3` builds Dovecot 2.3 modules (`lib20_` prefix, plugin-block based
settings) and installs the `packaging/xaps-23.conf` template as `95-xaps.conf`.

All CI builds and tests run inside the official Ubuntu Docker containers matching each Dovecot major (`ubuntu:26.04` for
2.4, `ubuntu:24.04` for 2.3); nothing is built on the CI host itself.

The installation paths currently target Debian and Ubuntu's Dovecot module layout under `/usr/lib/dovecot/modules`.

Testing
-------

The test suite builds a standalone test binary for each plugin module using lightweight Dovecot stubs, so it does not
require a running Dovecot installation. In CI it runs inside Ubuntu containers; locally you can replicate it in one:

```sh
docker run --rm --platform linux/amd64 \
  -v "$PWD":/src -w /src ubuntu:26.04 bash -c '
  apt-get update && apt-get install --yes build-essential cmake
  cmake -S tests -B build-test -DCMAKE_BUILD_TYPE=Release
  cmake --build build-test --parallel
  ctest --test-dir build-test --output-on-failure
'
```

The same verification (plugin build, Debian package, lintian) runs on both x64 and arm64 — for Dovecot 2.4 inside
`ubuntu:26.04` and for Dovecot 2.3 inside `ubuntu:24.04` — in the `Test` workflow by switching the platform:

```sh
docker run --rm --platform linux/arm64 -v "$PWD":/src -w /src ubuntu:26.04 bash -c '...'
```

Coverage build (used in CI):

```sh
cmake -S tests -B build-cov -DCMAKE_C_FLAGS="--coverage" -DCMAKE_EXE_LINKER_FLAGS="--coverage"
cmake --build build-cov --parallel
ctest --test-dir build-cov --output-on-failure
```

Build a Debian package
----------------------

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DXAPS_VERSION=1.0.0 \
  -DXAPS_DOVECOT_MAJOR=2.3
cmake --build build --parallel
cpack --config build/CPackConfig.cmake -G DEB -B dist
```

`-DXAPS_DOVECOT_MAJOR=2.4` (default) produces `dovecot-xaps-plugin_<version>-1_<arch>.deb`; `=2.3` produces
`dovecot-xaps-plugin_<version>-1~dov23_<arch>.deb` so the two builds never collide.

Or build the package entirely inside the matching Ubuntu Docker container, exactly as CI does:

```sh
docker run --rm --platform linux/amd64 \
  -v "$PWD":/src -w /src ubuntu:24.04 bash -c '
  apt-get update && apt-get install --yes build-essential cmake dovecot-core dovecot-dev lintian
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DXAPS_VERSION=1.0.0 -DXAPS_DOVECOT_MAJOR=2.3
  cmake --build build --parallel
  cpack --config build/CPackConfig.cmake -G DEB -B dist
'
```

CI builds and validates both Dovecot major versions in containers: the 2.4 package inside `ubuntu:26.04`, the 2.3
package inside `ubuntu:24.04`. Pushing a stable tag such as `v1.1.0` builds the Debian packages in the same containers
and attaches them to the corresponding GitHub release.

Troubleshooting
===============

If Dovecot reports `connect(127.0.0.1:11619) failed: Connection refused` while xapsd is listening on `[::1]:11619`,
ensure `xaps_url` uses `http://[::1]:11619`.

Useful checks:

```sh
sudo doveconf -n
sudo systemctl status dovecot xapsd
sudo journalctl -u dovecot -u xapsd -n 100 --no-pager
sudo ss -ltnp | grep 11619
```

Report plugin issues in this repository's [issue tracker](https://github.com/timlaing/dovecot-xaps-plugin/issues).

Acknowledgements
================

This repository is a maintained fork of [freswa/dovecot-xaps-plugin](https://github.com/freswa/dovecot-xaps-plugin),
maintained by Frederik Schwan, which is itself based on the original
[st3fan/dovecot-xaps-plugin](https://github.com/st3fan/dovecot-xaps-plugin) by Stefan Arentz. Their design,
implementation, maintenance, and the work of all contributors made this fork possible.
