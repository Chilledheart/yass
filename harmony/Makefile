all:
	mkdir -p entry/libs/arm64-v8a entry/libs/armeabi-v7a entry/libs/x86_64
	cd ../build-harmony-arm64 && ninja yass && cd ..
	cp -v ../build-harmony-arm64/libyass.so entry/libs/arm64-v8a
	cd ../build-harmony-arm && ninja yass && cd ..
	cp -v ../build-harmony-arm/libyass.so entry/libs/armeabi-v7a
	cd ../build-harmony-x64 && ninja yass && cd ..
	cp -v ../build-harmony-x64/libyass.so entry/libs/x86_64

clean:
	rm -rf entry/libs
