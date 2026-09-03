# Repository guidance

## Project

This repository builds Dovecot 2.4 plugins implementing `XAPPLEPUSHSERVICE` and forwarding mail events to
[`timlaing/dovecot-xaps-daemon`](https://github.com/timlaing/dovecot-xaps-daemon). It is maintained from
[`freswa/dovecot-xaps-plugin`](https://github.com/freswa/dovecot-xaps-plugin); preserve upstream attribution.

## Development rules

- Keep compatibility with Dovecot 2.4.2 and later in the 2.4 series.
- Preserve the three installed modules and Debian/Ubuntu module paths defined in `CMakeLists.txt`.
- Keep the default daemon URL aligned with xapsd: `http://[::1]:11619`.
- Treat Dovecot configuration expansion and plugin load order as correctness-sensitive.
- Never commit APNS credentials, device tokens, account data, email data, or private server details.
- Keep package metadata and user-facing links pointed at `timlaing/dovecot-xaps-plugin`.

## Verification

Use Ubuntu 26.04 with Dovecot 2.4.2 for authoritative checks:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
cpack --config build/CPackConfig.cmake -G DEB -B dist
lintian --fail-on error dist/*.deb
sudo dpkg --install dist/*.deb
sudo doveconf -n
```

Report checks as pass, fail, or not run. Do not imply macOS compilation proves Dovecot compatibility.

## Reviews and changes

- Use `.github/skills/code-review/SKILL.md` for formal reviews.
- Keep review findings scoped to the requested diff and rank real defects P0–P3.
- Preserve administrator-owned conffiles during package upgrades.
- Keep lint, tests, SonarCloud, Release Drafter, and release automation as separate workflows.
- Do not commit, push, create releases, or merge without explicit authorization.
