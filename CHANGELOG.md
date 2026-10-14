# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project follows
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.1] - 2026-10-14

### Added

- Pre-commit tooling (`prek` with cspell, prettier, ruff, yamllint, and YAML checks) plus the
  `.vscode` workspace spell-check and settings configuration.

### Changed

- Updated the README to use the renamed Ubuntu APT suites (`resolute` for Dovecot 2.4, `noble` for
  Dovecot 2.3).
- Refactored the registration and settings paths for const-correctness and simpler control flow in
  response to static analysis, with no change to runtime behaviour.

### Fixed

- Resolved the SonarQube findings in the IMAP, push-notification, settings, and utils modules.
- Hardened CI and package validation: package contents are now captured once before `grep` so a
  broken pipe cannot fail the build spuriously, the Dovecot 2.3 package check fails explicitly if
  the 2.4-only `libxaps_settings.so` leaks in, and cross-architecture installs pin `libsystemd0`
  and `libudev1` to the native version so an in-flight Ubuntu security push cannot break the
  arm64 jobs.

## [1.1.0] - 2026-09-06

### Added

- Unit tests for the XAPPLEPUSHSERVICE IMAP, push-notification, settings, and utils modules.
- Standalone test build using lightweight Dovecot stubs (no running Dovecot required).
- Coverage build with branch and line coverage measurement via gcov.
- Debian package build and release automation for Dovecot 2.4.2.
- Lint, package-validation, SonarQube Cloud, Dependabot, and Release Drafter automation.
- Contribution, security, conduct, funding, issue, and pull-request community files.
- Dovecot 2.3 support via `-DXAPS_DOVECOT_MAJOR=2.3`, which builds the `lib20_` modules, reads settings from the
  `plugin { }` block, and produces `...-1~dov23` Debian packages on Ubuntu 24.04. Dovecot 2.4 remains the default.

### Changed

- Updated the default Dovecot configuration to use 2.4 boolean-list plugin syntax.
- Changed the default xapsd endpoint to the IPv6 loopback address `[::1]:11619`.
- Updated package metadata and documentation to reference the maintained `timlaing` repositories.
- Connected tagged releases to the categorized draft produced by Release Drafter.

### Fixed

- Kept the parsed `xaps_url` in a plugin-owned pool so it outlives the transient transaction/command pool (use-after-free fix).
- Copied the `aps-topic` returned by the daemon into the plugin pool instead of pointing into the response stream buffer.
- Bounded `XAPPLEPUSHSERVICE` argument parsing so fewer than ten client arguments cannot cause out-of-bounds reads.
- `enable_testing()` now runs at the top level, so the Dovecot-stub unit tests execute via `ctest` in CI.
- Stopped logging device tokens and account IDs in the registration debug message.
- Replaced assertions with graceful error handling so a missing `xaps_url` no longer aborts Dovecot.
- Fixed a `string_t` leak on the registration error path.
