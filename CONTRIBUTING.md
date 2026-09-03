# Contributing to dovecot-xaps-plugin

Thank you for contributing. This repository is maintained at
[github.com/timlaing/dovecot-xaps-plugin](https://github.com/timlaing/dovecot-xaps-plugin) and builds on the work of
[freswa/dovecot-xaps-plugin](https://github.com/freswa/dovecot-xaps-plugin),
[st3fan/dovecot-xaps-plugin](https://github.com/st3fan/dovecot-xaps-plugin), and their contributors.

## Before you start

Search the existing issues and pull requests before opening a new one. Bugs in the companion daemon belong in the
[dovecot-xaps-daemon issue tracker](https://github.com/timlaing/dovecot-xaps-daemon/issues).

## Development setup

The supported build environment is Ubuntu 26.04 LTS with Dovecot 2.4.2 or newer.

```sh
sudo apt-get update
sudo apt-get install build-essential cmake dovecot-core dovecot-dev
git clone https://github.com/timlaing/dovecot-xaps-plugin.git
cd dovecot-xaps-plugin
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
```

Do not run the installation step on a production mail server while developing.

## Making changes

1. Fork the repository and create a focused branch such as `feature/my-change` or `fix/my-bug`.
2. Keep each pull request limited to one self-contained change.
3. Follow the existing C99 style and build without new compiler warnings.
4. Update `README.md` and `xaps.conf` when behavior or configuration changes.
5. Do not commit APNS keys, credentials, mail data, or private server details.

## Verification

Before submitting a pull request, run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
cpack --config build/CPackConfig.cmake -G DEB -B dist
```

Configuration changes must also be validated on Dovecot 2.4:

```sh
sudo doveconf -n
```

Describe any checks you could not run and why. GitHub Actions builds and inspects the Debian package on Ubuntu 26.04.

## Pull requests

Complete the pull-request template, link related issues with `Fixes #123` where appropriate, and allow maintainer
edits. Pull-request titles may use conventional prefixes such as `feat:`, `fix:`, `docs:`, or `chore:` so
Release Drafter can categorize the change.
