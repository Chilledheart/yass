## Prepare stage
download [commandline tools][cmdlinetools] and setup up environment variable `ANDROID_HOME_SDK` and `ANDROID_NDK_ROOT`.

Required Steps:
```
sdkmanager 'platforms;android-34'
```

Accepting licenses is required.

Optional Steps:
```
sdkmanager --install 'system-images;android-34;google_apis;x86_64'
sdkmanager --install 'ndk;25.2.9519653'
```

## Prepare stage (tun2proxy)
Required Steps:
Install [rustup.rs]
```
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
./scripts/setup-android-rust.sh
```

## Build stage

```
WITH_OS=android WITH_CPU=x64 ./scripts/build-tun2proxy.sh
WITH_OS=android WITH_CPU=arm ./scripts/build-tun2proxy.sh
WITH_OS=android WITH_CPU=arm64 ./scripts/build-tun2proxy.sh
WITH_OS=android WITH_CPU=x86 ./scripts/build-tun2proxy.sh
```

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
[rustup.rs]: https://rustup.rs/
