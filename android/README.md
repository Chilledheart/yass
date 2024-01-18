## Prepare stage
download [commandline tools][cmdlinetools] and setup up environment variable `ANDROID_HOME_SDK` and `ANDROID_NDK_ROOT`.

Required Steps:
```
sdkmanager 'platforms;android-30'
```

Accepting licenses is required.

Optional Steps:
```
sdkmanager --install 'system-images;android-30;google_apis;x86_64'
sdkmanager --install 'ndk;25.2.9519653'
```

## Build stage

```
./tools/build --variant gui --arch x64 --system android --cmake-build-type MinSizeRel -build-benchmark -build-test -nc
./tools/build --variant gui --arch arm --system android --cmake-build-type MinSizeRel -build-benchmark -build-test -nc
./tools/build --variant gui --arch arm64 --system android --cmake-build-type MinSizeRel -build-benchmark -build-test -nc
./tools/build --variant gui --arch x86 --system android --cmake-build-type MinSizeRel -build-benchmark -build-test -nc
```

For debugging purposes:
```
./gradlew assembleDebug --info
```
or
```
./gradlew assembleRelease --info
```

[cmdlinetools]: https://developer.android.com/studio#command-line-tools-only
