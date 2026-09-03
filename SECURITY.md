# Security Policy

## Supported versions

Only the latest release is supported with security fixes.

| Version | Supported |
| ------- | --------- |
| Latest  | Yes       |
| Older   | No        |

## Reporting a vulnerability

Do not report vulnerabilities in the public issue tracker. Use
[GitHub private vulnerability reporting](https://github.com/timlaing/dovecot-xaps-plugin/security/advisories/new)
so the report goes directly to the maintainers.

Include the affected version, Dovecot version, impact, reproduction steps, and any suggested mitigation. Remove APNS
credentials, device tokens, email addresses, and private server details.

Maintainers will acknowledge the report as soon as practical, coordinate remediation and disclosure with the reporter,
and provide credit if requested.

## Scope

This policy covers the Dovecot XAPS plugin and its packaged configuration. Vulnerabilities in Dovecot, xapsd, Apple
services, or the host operating system should be reported to their respective maintainers.

## Security tooling

The repository uses compiler warnings, Dependabot, SonarQube Cloud, and GitHub security scanning. These automated
checks complement, but do not replace, security review.
