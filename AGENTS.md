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
- Reference GitHub Actions by version tag (for example `actions/checkout@v7.0.1`), not by commit SHA, and keep every reference to the same action on one tag.
- Update `CHANGELOG.md` under the "Unreleased" section for every PR with a user-facing or behavior change (Added/Changed/Fixed/Removed/Deprecated/Security). Include a concise entry and reference the PR number. Do not update `CHANGELOG.md` for purely internal/CI/tooling changes unless they affect users or packaging.
- When adding words to cspell dictionaries (e.g., `.vscode/cspell.json`), ensure words are in lowercase and in alphabetical order.
- Never bypass commit checks (e.g., `git commit --no-verify`). All pre-commit hooks must pass locally before committing changes. If a hook fails due to false positives, fix the configuration or the files properly — do not skip the checks. Only bypass checks with explicit case-by-case approval from the user.
- To release, rename the `[Unreleased]` heading to `[VERSION] - YYYY-MM-DD`; leave no empty `[Unreleased]` section behind, and do not add a comparison-links footer.
- Cut the release by publishing the draft release that Release Drafter maintains (`.github/workflows/release-drafter.yml`): set its tag and title to `vMAJOR.MINOR.PATCH` (the version from the `CHANGELOG.md` heading prefixed with `v`); the tag must be `v*.*.*` to match the release build trigger (`.github/workflows/release.yml`).
- Do not commit, push, create releases, or merge without explicit authorization.
