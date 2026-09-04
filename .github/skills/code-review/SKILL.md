---
name: code-review
description: Review changes, pull requests, or commits in dovecot-xaps-plugin for Dovecot compatibility, C correctness, configuration safety, Debian packaging, tests, and release automation.
---

# Code Review — dovecot-xaps-plugin

Apply this checklist when reviewing this repository.

## Review standard

- Establish the target before reviewing: working tree, staged changes, commit range, or PR against its merge base. State the target and base.
- Review only changed behavior. Do not present pre-existing issues as findings unless the change materially worsens them.
- Report only demonstrable functional, security, reliability, compatibility, packaging, or maintainability defects. Give the triggering conditions, impact, and smallest useful `file:line` range.
- Do not invent findings or elevate style preferences. Keep optional improvements separate.
- Rank findings as P0 catastrophic/release-blocking, P1 likely serious defect, P2 real defect under plausible conditions, or P3 worthwhile non-blocking defect.

## Project invariants

- Target Dovecot 2.4.2 and later within the 2.4 series. Treat Dovecot API or module-layout assumptions as compatibility-sensitive.
- Preserve the separate IMAP, push-notification, and settings modules and their installed names under `/usr/lib/dovecot/modules`.
- Dovecot configuration must expand variables intentionally. A literal `$mail_plugins` token in a module list causes Dovecot to look for a plugin with that name.
- The default daemon endpoint is `http://[::1]:11619`; do not silently change its address family independently of xapsd.
- Validate all external data used in notification requests. Review allocation sizes, ownership, null handling, escaping, HTTP status handling, and cleanup on every error path.
- Avoid logging credentials, device tokens, account identifiers, email data, or private topology.

## Build, packaging, and configuration

- Configure and build with warnings enabled. Changed C should not introduce compiler warnings.
- Confirm CPack metadata uses this repository's canonical URL and declares runtime dependencies.
- Preserve `/etc/dovecot/conf.d/95-xaps.conf` as a conffile so package upgrades do not overwrite administrator changes.
- A generated `.deb` must contain all three modules and the Dovecot configuration file.
- Run `lintian --fail-on error` on the generated package. Inspect warnings separately rather than claiming the package is warning-free.
- When configuration changes, install the package in a disposable Dovecot 2.4 environment and run `doveconf -n`.

## Workflows and releases

- Lint, test, SonarCloud, Release Drafter, and release workflows must remain separate and use least-privilege permissions.
- Tagged release versions must match `vMAJOR.MINOR.PATCH` and the Debian package version.
- Releases must publish the `.deb`, `SHA256SUMS`, and SPDX JSON SBOM, using the existing Release Drafter release notes.
- Do not expose `SONAR_TOKEN` or other secrets to untrusted fork pull requests.

## Verification

Run checks proportionate to the change, normally in Ubuntu 26.04 with Dovecot 2.4.2:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
cpack --config build/CPackConfig.cmake -G DEB -B dist
lintian --fail-on error dist/*.deb
```

For configuration/package changes, also install the package and run `doveconf -n`. Record every check as pass, fail, or not run, including environmental limitations.

## Review output

1. List findings first in severity order with tight file and line references.
2. Explain why each issue is reachable and what fails.
3. Separate non-blocking suggestions from findings.
4. End with verification evidence and limitations.
5. If nothing qualifies, say explicitly that no actionable findings were found.

A review request is read-only. Do not edit, commit, push, resolve discussions, or merge unless the user explicitly asks.
