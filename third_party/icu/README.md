# Update Me

```
git clone https://chromium.googlesource.com/chromium/deps/icu
```

Copy `android`, `common`, `ios`, `source\i18n`, `source\common` and `source\stubdata` directories, excluding `icudtb.dat`

```
rm -rf source
mkdir source
cp -rv ~/icu/android ~/icu/common ~/icu/ios .
cp -rv ~/icu/source/i18n ~/icu/source/common ~/icu/source/stubdata source
rm -v common/icudtb.dat
```
