# Pull Request

<!-- Thank you for contributing to dovecot-xaps-plugin. Please complete this template. -->

## Summary

<!-- Briefly describe the change and the problem it solves. -->

## Type of change

- [ ] 🚀 Feature (`feature` / `enhancement`)
- [ ] 🐛 Bug fix (`bug` / `fix`)
- [ ] 🛠 Maintenance (`maintenance` / `dependencies`)
- [ ] 📚 Documentation (`documentation`)

## Related issues / pull requests

<!-- Link related work, for example: Fixes #123. -->

## Changes

<!-- Explain the implementation and any Dovecot configuration or compatibility impact. -->

## Verification

- [ ] This pull request contains one self-contained change.
- [ ] Maintainers are allowed to edit this pull request.
- [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr` succeeds.
- [ ] `cmake --build build --parallel` succeeds.
- [ ] The Debian package builds with CPack.
- [ ] `doveconf -n` succeeds on Dovecot 2.4, if configuration changed.
- [ ] Documentation was updated where needed.

<!-- List any checks that were not run and explain why. -->
