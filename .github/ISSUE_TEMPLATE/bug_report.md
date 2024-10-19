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

#### Re-run with assertions (optional)

See [the guide](https://github.com/Chilledheart/yass/blob/develop/BUILDING.md) to build *Debug* or *RelWithDebInfo* version.

If you face some crash and the log is somehow incomplete, this variant will run more checks and produce more useful logs.

#### yass coredump (optional)

See [the guide](https://github.com/Chilledheart/yass/wiki/Debug-Guide#check-coredump) to enable coredump in your system.

Attach your coredump file in the issue
