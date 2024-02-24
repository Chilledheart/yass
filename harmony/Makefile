all:
	mkdir -p entry/libs/arm64-v8a entry/libs/armeabi-v7a entry/libs/x86_64
	cd ../build-harmony-arm64 && rm -f libyass.so && cd ..
	cd ../build-harmony-arm64 && ninja yass && cd ..
	cd ../build-harmony-arm64 && ../third_party/llvm-build/Release+Asserts/bin/llvm-objcopy --only-keep-debug libyass.so libyass.so.dbg && cd ..
	cd ../build-harmony-arm64 && ../third_party/llvm-build/Release+Asserts/bin/llvm-objcopy --strip-debug libyass.so && cd ..
	cd ../build-harmony-arm64 && ../third_party/llvm-build/Release+Asserts/bin/llvm-objcopy --add-gnu-debuglink=libyass.so.dbg libyass.so && cd ..
	cp -v ../build-harmony-arm64/libyass.so entry/libs/arm64-v8a
	cd ../build-harmony-arm && rm -f libyass.so && cd ..
	cd ../build-harmony-arm && ninja yass && cd ..
	cd ../build-harmony-arm && ../third_party/llvm-build/Release+Asserts/bin/llvm-objcopy --only-keep-debug libyass.so libyass.so.dbg && cd ..
	cd ../build-harmony-arm && ../third_party/llvm-build/Release+Asserts/bin/llvm-objcopy --strip-debug libyass.so && cd ..
	cd ../build-harmony-arm && ../third_party/llvm-build/Release+Asserts/bin/llvm-objcopy --add-gnu-debuglink=libyass.so.dbg libyass.so && cd ..
	cp -v ../build-harmony-arm/libyass.so entry/libs/armeabi-v7a
	cd ../build-harmony-x64 && rm -f libyass.so && cd ..
	cd ../build-harmony-x64 && ninja yass && cd ..
	cd ../build-harmony-x64 && ../third_party/llvm-build/Release+Asserts/bin/llvm-objcopy --only-keep-debug libyass.so libyass.so.dbg && cd ..
	cd ../build-harmony-x64 && ../third_party/llvm-build/Release+Asserts/bin/llvm-objcopy --strip-debug libyass.so && cd ..
	cd ../build-harmony-x64 && ../third_party/llvm-build/Release+Asserts/bin/llvm-objcopy --add-gnu-debuglink=libyass.so.dbg libyass.so && cd ..
	cp -v ../build-harmony-x64/libyass.so entry/libs/x86_64

clean:
	rm -rf entry/libs
