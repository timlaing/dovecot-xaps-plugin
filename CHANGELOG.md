# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project follows
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Unit tests for the XAPPLEPUSHSERVICE IMAP, push-notification, settings, and utils modules.
- Standalone test build using lightweight Dovecot stubs (no running Dovecot required).
- Coverage build with branch and line coverage measurement via gcov.
- Debian package build and release automation for Dovecot 2.4.2.
- Lint, package-validation, SonarQube Cloud, Dependabot, and Release Drafter automation.
- Contribution, security, conduct, funding, issue, and pull-request community files.

### Changed

- Updated the default Dovecot configuration to use 2.4 boolean-list plugin syntax.
- Changed the default xapsd endpoint to the IPv6 loopback address `[::1]:11619`.
- Updated package metadata and documentation to reference the maintained `timlaing` repositories.
- Connected tagged releases to the categorized draft produced by Release Drafter.
