---
name: Bug report
about: Create a report to help us improve
title: ""
labels: "bug"
---

### Environment

Include the result of the following commands:
  - `yass --version` or the screenshot of About Dialog (for desktop users)
  - screenshot of apk version or ipk version (for mobile users)

### Description

Describe the bug in full detail including expected and actual behavior.
Specify conditions that caused it. Provide the relevant part of yass
configuration and debug log.

- [ ] The bug is reproducible with the latest version of yass
- [ ] The yass configuration is minimized to the smallest possible
to reproduce the issue.

#### yass configuration

```
# Your yass configuration here
```
or share the configuration in [gist](https://gist.github.com/).

- [ ] output of `reg query HKCU\SOFTWARE\YetAnotherShadowSocket` (for windows)
- [ ] output of `cat ~/.yass/config.json` (for linux/freebsd)
- [ ] output of `plutil -p ~/Library/Preferences/it.gui.yass.suite.plist` (for macos)
- [ ] screenshot of yass configure window (for android/ios)

#### yass debug log

It is advised to enable

- [ ] yass.exe....INFO... file under `%TMP%` directory (windows)
- [ ] yass.INFO file under `$TMPDIR` directory (non-windows)
```
# Your yass debug log here
```
or share the debug log in [gist](https://gist.github.com/).
