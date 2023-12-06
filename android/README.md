## Prepare stage
download [commandline tools][cmdlinetools] and setup `ANDROID_HOME_SDK`

Required Steps:
```
sdkmanager 'platforms;android-30'
```

Accepting licenses is required.

Optional Steps:
```
sdkmanager --install 'system-images;android-30;google_apis;x86_64'
sdkmanager --install 'ndk;21.4.7075529'
```

## Build stage

```
./tools/build --variant cli --arch x64 --system android --cmake-build-type MinSizeRel -build-benchmark -build-test -nc
./tools/build --variant cli --arch arm --system android --cmake-build-type MinSizeRel -build-benchmark -build-test -nc
./tools/build --variant cli --arch arm64 --system android --cmake-build-type MinSizeRel -build-benchmark -build-test -nc
./tools/build --variant cli --arch x86 --system android --cmake-build-type MinSizeRel -build-benchmark -build-test -nc
```

For debugging purposes:
```
./gradlew assembleDebug
```
or
```
./gradlew assembleRelease
```

[cmdlinetools]: https://developer.android.com/studio#command-line-tools-only
