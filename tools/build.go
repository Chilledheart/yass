package main

import (
	"archive/zip"
	"bufio"
	"encoding/base64"
	"errors"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"bytes"

	"github.com/golang/glog"
	"github.com/google/uuid"
)

var APPNAME = "yass"

var toolDir string
var projectDir string
var buildDir string
var tagFlag string
var fullTagFlag string

var dryRunFlag bool
var preCleanFlag bool
var noPackagingFlag bool
var buildBenchmarkFlag bool
var runBenchmarkFlag bool
var buildTestFlag bool
var runTestFlag bool
var verboseFlag int

var cmakeBuildTypeFlag string
var cmakeBuildConcurrencyFlag int

var useLibCxxFlag bool
var enableLtoFlag bool

var clangTidyModeFlag bool
var clangTidyExecutablePathFlag string

var macosxVersionMinFlag string
var macosxUniversalBuildFlag bool
var macosxCodeSignIdentityFlag string

var msvcTargetArchFlag string
var msvcCrtLinkageFlag string
var msvcAllowXpFlag bool

var freebsdAbiFlag int

var systemNameFlag string
var subSystemNameFlag string
var sysrootFlag string
var archFlag string

var variantFlag string

var androidAppAbi string
var androidAbiTarget string
var androidApiLevel int

func getAppName() string {
	if systemNameFlag == "windows" {
		return APPNAME + ".exe"
	} else if systemNameFlag == "darwin" {
		return APPNAME + ".app"
	} else {
		return APPNAME
	}
}

func getEnv(key string, fallback string) string {
	if value, ok := os.LookupEnv(key); ok {
		return value
	}
	return fallback
}

func getEnvBool(key string, fallback bool) bool {
	if value, ok := os.LookupEnv(key); ok {
		result, _ := strconv.ParseBool(value)
		return result
	}
	return fallback
}

func InitFlag() {
	_, filename, _, ok := runtime.Caller(1)

	if !ok {
		glog.Fatalf("source code not found")
	}

	toolDir = filepath.Dir(filename)
	projectDir = filepath.Dir(toolDir)

	// override defaults
	flag.Set("v", "2")
	flag.Set("alsologtostderr", "true")

	var flagNoPreClean bool

	flag.BoolVar(&dryRunFlag, "dry-run", false, "Generate build script but without actually running it")
	flag.BoolVar(&preCleanFlag, "pre-clean", true, "Clean the source tree before building")
	flag.BoolVar(&flagNoPreClean, "nc", false, "Don't Clean the source tree before building")
	flag.BoolVar(&noPackagingFlag, "no-packaging", false, "Skip packaging step")
	flag.BoolVar(&buildBenchmarkFlag, "build-benchmark", false, "Build benchmarks")
	flag.BoolVar(&runBenchmarkFlag, "run-benchmark", false, "Build and run benchmarks")
	flag.BoolVar(&buildTestFlag, "build-test", false, "Build unittests")
	flag.BoolVar(&runTestFlag, "run-test", false, "Build and run unittests")
	flag.IntVar(&verboseFlag, "verbose", 0, "Run unittests with verbose level")

	flag.StringVar(&cmakeBuildTypeFlag, "cmake-build-type", getEnv("BUILD_TYPE", "Release"), "Set cmake build configuration")
	flag.IntVar(&cmakeBuildConcurrencyFlag, "cmake-build-concurrency", runtime.NumCPU(), "Set cmake build concurrency")

	flag.BoolVar(&useLibCxxFlag, "use-libcxx", true, "Use Custom libc++")
	flag.BoolVar(&enableLtoFlag, "enable-lto", true, "Enable lto")

	flag.BoolVar(&clangTidyModeFlag, "clang-tidy-mode", getEnvBool("ENABLE_CLANG_TIDY", false), "Enable Clang Tidy Build")
	flag.StringVar(&clangTidyExecutablePathFlag, "clang-tidy-executable-path", getEnv("CLANG_TIDY_EXECUTABLE", ""), "Path to clang-tidy, only used by Clang Tidy Build")

	flag.StringVar(&macosxVersionMinFlag, "macosx-version-min", getEnv("MACOSX_DEPLOYMENT_TARGET", "10.14"), "Set Mac OS X deployment target, such as 10.15")
	flag.BoolVar(&macosxUniversalBuildFlag, "macosx-universal-build", getEnvBool("ENABLE_OSX_UNIVERSAL_BUILD", true), "Enable Mac OS X Universal Build")
	flag.StringVar(&macosxCodeSignIdentityFlag, "macosx-codesign-identity", getEnv("CODESIGN_IDENTITY", "-"), "Set Mac OS X CodeSign Identity")

	flag.StringVar(&msvcTargetArchFlag, "msvc-tgt-arch", getEnv("VSCMD_ARG_TGT_ARCH", "x64"), "Set Visual C++ Target Achitecture")
	flag.StringVar(&msvcCrtLinkageFlag, "msvc-crt-linkage", getEnv("MSVC_CRT_LINKAGE", "static"), "Set Visual C++ CRT Linkage")
	flag.BoolVar(&msvcAllowXpFlag, "msvc-allow-xp", getEnvBool("MSVC_ALLOW_XP", false), "Enable Windows XP Build")

	flag.IntVar(&freebsdAbiFlag, "freebsd-abi", getFreebsdABI(11), "Select FreeBSD ABI")

	flag.StringVar(&systemNameFlag, "system", runtime.GOOS, "Specify host system name")
	flag.StringVar(&subSystemNameFlag, "subsystem", "", "Specify host subsystem name")
	flag.StringVar(&sysrootFlag, "sysroot", "", "Specify host sysroot, used in cross-compiling")
	flag.StringVar(&archFlag, "arch", runtime.GOARCH, "Specify host architecture")

	flag.StringVar(&variantFlag, "variant", "gui", "Specify variant, available: gui, cli, server")

	flag.IntVar(&androidApiLevel, "android-api", 24, "Select Android API Level")

	flag.Parse()

	if flagNoPreClean {
		preCleanFlag = false
	}

	// clang-tidy complains about parse error
	if clangTidyModeFlag {
		macosxUniversalBuildFlag = false
		noPackagingFlag = true
	}

	// For compatiblity
	systemNameFlag = strings.ToLower(systemNameFlag)

	if (variantFlag == "gui") {
		APPNAME = "yass"
	} else if (variantFlag == "cli") {
		APPNAME = "yass_cli"
	} else if (variantFlag == "server") {
		APPNAME = "yass_server"
	} else if (variantFlag == "benchmark") {
		APPNAME = "yass_benchmark"
	} else if (variantFlag == "test") {
		APPNAME = "yass_test"
	} else {
		glog.Fatalf("Invalid variant: %s", variantFlag)
	}
}

func prebuildFindSourceDirectory() {
	glog.Info("PreStage -- Find Source Directory")
	glog.Info("======================================================================")
	err := os.Chdir(projectDir)
	if err != nil {
		glog.Fatalf("%v", err)
	}
	if _, err = os.Stat("CMakeLists.txt"); errors.Is(err, os.ErrNotExist) {
		glog.Fatalf("Cannot find top dir of the source tree")
	}

	if _, err = os.Stat(".git"); err == nil {
		cmd := exec.Command("git", "describe", "--abbrev=0", "--tags", "HEAD")
		var outb, errb bytes.Buffer
		cmd.Stdout = &outb
		cmd.Stderr = &errb
		err = cmd.Run()
		if err == nil {
			tagFlag = strings.TrimSpace(outb.String())
		}
	}

	if err != nil {
		tagContent, err := ioutil.ReadFile("TAG")
		if err != nil {
			glog.Fatalf("%v", err)
		}
		tagFlag = strings.TrimSpace(string(tagContent))
	}

	if _, err = os.Stat(".git"); err == nil {
		cmd := exec.Command("git", "describe", "--tags", "HEAD")
		var outb, errb bytes.Buffer
		cmd.Stdout = &outb
		cmd.Stderr = &errb
		err = cmd.Run()
		if err == nil {
			fullTagFlag = strings.TrimSpace(outb.String())
		}
	}

	if err != nil {
		var subtagContent []byte;
		subtagContent, err = ioutil.ReadFile("SUBTAG")
		if err != nil {
			glog.Fatalf("%v", err)
		}
		subtagFlag := strings.TrimSpace(string(subtagContent))

		var lastChangeContent []byte;
		lastChangeContent, err = ioutil.ReadFile("LAST_CHANGE")
		if err != nil {
			glog.Fatalf("%v", err)
		}
		lastChangeFlag := strings.TrimSpace(string(lastChangeContent))[:8]

		fullTagFlag = fmt.Sprintf("%s-%s-%s", tagFlag, subtagFlag, lastChangeFlag)
	}

	if systemNameFlag == "windows" && msvcTargetArchFlag != "" {
		osSuffix := ""
		if msvcAllowXpFlag {
			osSuffix = "-winxp"
		}
		buildDir = fmt.Sprintf("build-msvc%s-%s-%s", osSuffix, msvcTargetArchFlag, msvcCrtLinkageFlag)
	} else {
		arch := archFlag
		if systemNameFlag == "darwin" && macosxUniversalBuildFlag {
			arch = "universal"
		}
		buildDir = fmt.Sprintf("build-%s-%s", systemNameFlag, arch)
		if subSystemNameFlag != "" {
			buildDir = fmt.Sprintf("build-%s-%s-%s", systemNameFlag, subSystemNameFlag, arch)
		}
	}
	buildDir, err = filepath.Abs(buildDir)
	if err != nil {
		glog.Fatalf("%v", err)
	}

	glog.V(2).Infof("Set build directory %s", buildDir)
	if _, err := os.Stat(buildDir); err == nil {
		if preCleanFlag {
			err := os.RemoveAll(buildDir)
			if err != nil {
				glog.Fatalf("%v", err)
			}
			glog.V(2).Infof("Removed previous build directory %s", buildDir)
		}
	}
	if _, err := os.Stat(buildDir); errors.Is(err, os.ErrNotExist) {
		err := os.Mkdir(buildDir, 0777)
		if err != nil {
			glog.Fatalf("%v", err)
		}
	}
	glog.V(1).Infof("Changed to build directory %s", buildDir)
	err = os.Chdir(buildDir)
	if err != nil {
		glog.Fatalf("%v", err)
	}
}

func getLLVMTargetTripleMSVC(msvcTargetArch string) string {
	if msvcTargetArch == "x86" {
		return "i686-pc-windows-msvc"
	} else if msvcTargetArch == "x64" {
		return "x86_64-pc-windows-msvc"
	} else if msvcTargetArch == "arm" {
		// lld-link doesn't accept this triple
		// cmake_args.extend(["-DCMAKE_LINKER=link"])
		return "arm-pc-windows-msvc"
	} else if msvcTargetArch == "arm64" {
		return "arm64-pc-windows-msvc"
	}
	glog.Fatalf("Invalid msvcTargetArch: %s", msvcTargetArch)
	return ""
}

// build/config/android/abi.gni
func getAndroidTargetAndAppAbi(arch string) (string, string) {
	if arch == "x64" {
		return "x86_64-linux-android", "x86_64"
	} else if arch == "x86" {
		return "i686-linux-android", "x86"
	} else if arch == "arm64" {
		return "aarch64-linux-android", "arm64-v8a"
	} else if arch == "arm" {
		// armeabi for armv6
		return "arm-linux-androideabi", "armeabi-v7a"
	} else if arch == "mipsel" {
		return "mipsel-linux-android", "mips"
	} else if arch == "mips64el" {
		return "mips64el-linux-android", "mips64"
	} else if arch == "riscv64" {
		return "riscv64-linux-android", "riscv64"
	}
	glog.Fatalf("Invalid arch: %s", arch)
	return "", ""
}

// https://docs.rust-embedded.org/embedonomicon/compiler-support.html
func getGNUTargetTypeAndArch(arch string, subsystem string) (string, string) {
	if arch == "amd64" || arch == "x86_64" {
		if subsystem == "musl" {
			return "x86_64-linux-musl", "x86_64"
		}
		return "x86_64-linux-gnu", "x86_64"
	} else if arch == "x86" {
		if subsystem == "musl" {
			return "i686-linux-musl", "i686"
		}
		return "i686-linux-gnu", "i686"
	} else if arch == "i386" || arch == "i486" || arch == "i586" || arch == "i686" {
		if subsystem == "musl" {
			return fmt.Sprintf("%s-linux-musl", arch), arch
		}
		return fmt.Sprintf("%s-linux-gnu", arch), arch
	} else if arch == "arm64" || arch == "aarch64" {
		if subsystem == "musl" {
			return "aarch64-linux-musl", "aarch64"
		}
		return "aarch64-linux-gnu", "aarch64"
	} else if arch == "armel" {
		if subsystem == "musl" {
			return "arm-linux-musleabi", "armel"
		}
		return "arm-linux-gnueabi", "armel"
	} else if arch == "arm" || arch == "armhf" {
		if subsystem == "musl" {
			return "arm-linux-musleabihf", "armhf"
		}
		return "arm-linux-gnueabihf", "armhf"
	} else if arch == "mips" {
		if subsystem == "musl" {
			return "mips-linux-musl", "mips"
		}
		return "mips-linux-gnu", "mips"
	} else if arch == "mips64" {
		if subsystem == "musl" {
			return "mips64-linux-muslabi64", "mips64"
		}
		return "mips64-linux-gnuabi64", "mips64"
	} else if arch == "mipsel" {
		if subsystem == "musl" {
			return "mipsel-linux-musl", "mipsel"
		}
		return "mipsel-linux-gnu", "mipsel"
	} else if arch == "mips64el" {
		if subsystem == "musl" {
			return "mips64el-linux-muslabi64", "mips64el"
		}
		return "mips64el-linux-gnuabi64", "mips64el"
	}
	glog.Fatalf("Invalid arch: %s", arch)
	return "", ""
}

func getAndFixLibunwind() {
	// ln -sf $PWD/third_party/android_toolchain/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/14.0.7/lib/linux/i386 third_party/llvm-build/Release+Asserts/lib/clang/18/lib/linux
	entries, err := os.ReadDir("../third_party/llvm-build/Release+Asserts/lib/clang")
	if err != nil {
		glog.Fatalf("%v", err)
	}
	var llvm_version string
	for _, entry := range entries {
		llvm_version = entry.Name()
	}
	if llvm_version == "" {
		glog.Fatalf("toolchain not found")
	}
	source_path := "../third_party/android_toolchain/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/14.0.7/lib/linux"
	source_path, err = filepath.Abs(source_path)
	if err != nil {
		glog.Fatalf("%v", err)
	}
	target_path := fmt.Sprintf("../third_party/llvm-build/Release+Asserts/lib/clang/%s/lib/linux", llvm_version)
	entries, err = os.ReadDir(source_path)
	if err != nil {
		glog.Fatalf("%v", err)
	}
	for _, entry := range entries {
		if _, err = os.Stat(filepath.Join(target_path, entry.Name())); err == nil {
			err = os.Remove(filepath.Join(target_path, entry.Name()))
			if err != nil {
				glog.Fatalf("%v", err)
			}
		}
		err = os.Symlink(filepath.Join(source_path, entry.Name()),
				filepath.Join(target_path, entry.Name()))
		if err != nil {
			glog.Fatalf("%v", err)
		}
		glog.Info("Created symbolic links at ", filepath.Join(target_path, entry.Name()))
	}
}

func cmdStdinPipe(c *exec.Cmd) (*os.File, *os.File, error) {
	if c.Stdin != nil {
		return nil, nil, errors.New("exec: Stdin already set")
	}
	if c.Process != nil {
		return nil, nil, errors.New("exec: cmdStdinPipe after process started")
	}
	pr, pw, err := os.Pipe()
	if err != nil {
		return nil, nil, err
	}
	c.Stdin = pr
	return pw, pr, nil
}

func cmdStdoutStderrPipe(c *exec.Cmd) (*os.File, *os.File, error) {
	if c.Stdout != nil {
		return nil, nil, errors.New("exec: Stdout already set")
	}
	if c.Stderr != nil {
		return nil, nil, errors.New("exec: Stderr already set")
	}
	if c.Process != nil {
		return nil, nil, errors.New("exec: cmdStdoutStderrPipe after process started")
	}
	pr, pw, err := os.Pipe()
	if err != nil {
		return nil, nil, err
	}
	c.Stdout = pw
	c.Stderr = pw
	return pr, pw, nil
}

func cmdRun(args []string, check bool) {
	var cmd = exec.Command(args[0], args[1:]...)
	glog.V(2).Infof("Running command \"%v\"", args)
	if dryRunFlag {
		return
	}
	var stdin, stdinPipe, stdouterr, stdouterrPipe *os.File
	var err error
	stdin, stdinPipe, err = cmdStdinPipe(cmd)
	if err != nil {
		if !check {
			return
		}
		glog.Fatalf("%v", err)
	}
	stdouterr, stdouterrPipe, err = cmdStdoutStderrPipe(cmd)
	if err != nil {
		if !check {
			return
		}
		glog.Fatalf("%v", err)
	}
	err = cmd.Start()
	err = stdin.Close()
	if err != nil {
		if !check {
			return
		}
		glog.Fatalf("%v", err)
	}
	err = stdinPipe.Close()
	if err != nil {
		if !check {
			return
		}
		glog.Fatalf("%v", err)
	}
	err = stdouterrPipe.Close()
	if err != nil {
		if !check {
			return
		}
		glog.Fatalf("%v", err)
	}
	if err != nil {
		glog.Warningf("Command \"%v\" failed with %v", args, err)
		if !check {
			return
		}
		glog.Fatalf("%v", err)
	}
	scanner := bufio.NewScanner(stdouterr)
	scanner.Split(bufio.ScanLines)
	for scanner.Scan() {
		glog.Info(scanner.Text())
	}
	err = cmd.Wait()
	if err != nil {
		glog.Warningf("Command \"%v\" failed with %v", args, err)
		if !check {
			return
		}
		glog.Fatalf("%v", err)
	}
	err = stdouterr.Close()
	if err != nil {
		if !check {
			return
		}
		glog.Fatalf("%v", err)
	}
}

func cmdCheckOutput(args []string) string {
	var cmd = exec.Command(args[0], args[1:]...)
	glog.V(2).Infof("Running command \"%v\"", args)
	output, err := cmd.Output()
	if err != nil {
		glog.Fatalf("%v", err)
	}
	return string(output)
}

func buildStageGenerateBuildScript() {
	glog.Info("BuildStage -- Generate Build Script")
	glog.Info("======================================================================")
	var cmakeArgs []string
	cmakeArgs = append(cmakeArgs, "-DGUI=ON", "-DCLI=ON", "-DSERVER=ON")
	cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_BUILD_TYPE=%s", cmakeBuildTypeFlag))
	cmakeArgs = append(cmakeArgs, "-G", "Ninja")
	if systemNameFlag != runtime.GOOS || sysrootFlag != "" || msvcTargetArchFlag != "x64" {
		cmakeArgs = append(cmakeArgs, "-DUSE_HOST_TOOLS=on")
	}
	if subSystemNameFlag == "musl" || subSystemNameFlag == "openwrt" {
		cmakeArgs = append(cmakeArgs, "-DUSE_MUSL=on")
	}
	if buildBenchmarkFlag || runBenchmarkFlag {
		cmakeArgs = append(cmakeArgs, "-DBUILD_BENCHMARKS=on")
	}
	if buildTestFlag || runTestFlag {
		cmakeArgs = append(cmakeArgs, "-DBUILD_TESTS=on")
	}
	if useLibCxxFlag {
		cmakeArgs = append(cmakeArgs, "-DUSE_LIBCXX=on")
	} else {
		cmakeArgs = append(cmakeArgs, "-DUSE_LIBCXX=off")
	}
	if enableLtoFlag {
		cmakeArgs = append(cmakeArgs, "-DENABLE_LTO=on")
	} else {
		cmakeArgs = append(cmakeArgs, "-DENABLE_LTO=off")
	}
	if clangTidyModeFlag {
		cmakeArgs = append(cmakeArgs, "-DENABLE_CLANG_TIDY=on", fmt.Sprintf("-DCLANG_TIDY_EXECUTABLE=%s", clangTidyExecutablePathFlag))
	}
	if systemNameFlag == "windows" {
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCROSS_TOOLCHAIN_FLAGS_TOOLCHAIN_FILE=%s\\Native.cmake", buildDir))
		// Guesting native LIB paths from host LIB paths
		var nativeLibPaths []string
		hostLibPaths := strings.Split(os.Getenv("LIB"), ";")
		for _, hostLibPath := range hostLibPaths {
			var nativeLibPath string
			if strings.Contains(strings.ToLower(hostLibPath), "v7.1a") {
				// Old Windows SDK looks like:
				// C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Lib
				// C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Lib\x64
				if strings.HasSuffix(strings.ToLower(hostLibPath), "lib") {
					nativeLibPath = filepath.Join(hostLibPath, "x64")
				} else {
					nativeLibPath = hostLibPath
				}

			} else {
				nativeLibPath = filepath.Join(filepath.Dir(hostLibPath), "x64")
			}
			nativeLibPaths = append(nativeLibPaths, strings.Replace(nativeLibPath, "\\", "/", -1))
		}
		nativeLIB := strings.Join(nativeLibPaths, ";")
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCROSS_TOOLCHAIN_FLAGS_NATIVE_LIB=%s", nativeLIB))

		useMsvcCl := getEnv("CC", "cl") == "cl"
		ccCompiler := getEnv("CC", "cl")
		cxxCompiler := getEnv("CXX", "cl")
		// Retarget cl based on current path
		if useMsvcCl {
			path, err := exec.LookPath(ccCompiler)
			if err != nil {
				fmt.Println("Could not find path of cl")
				glog.Fatalf("%v", err)
			}
			ccCompiler = filepath.Join(filepath.Dir(path), "..", "x64", "cl.exe")
			cxxCompiler = filepath.Join(filepath.Dir(path), "..", "x64", "cl.exe")
		}
		nativeToolChainContent := strings.Replace(fmt.Sprintf("set(CMAKE_C_COMPILER \"%s\")\n", ccCompiler), "\\", "/", -1)
		nativeToolChainContent += strings.Replace(fmt.Sprintf("set(CMAKE_CXX_COMPILER \"%s\")\n", cxxCompiler), "\\", "/", -1)
		nativeToolChainContent += "set(CMAKE_C_COMPILER_TARGET \"x86_64-pc-windows-msvc\")\n"
		nativeToolChainContent += "set(CMAKE_CXX_COMPILER_TARGET \"x86_64-pc-windows-msvc\")\n"
		err := ioutil.WriteFile("Native.cmake", []byte(nativeToolChainContent), 0666)
		if err != nil {
			glog.Fatalf("%v", err)
		}

		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DMSVC_CRT_LINKAGE=%s", msvcCrtLinkageFlag))
		if msvcAllowXpFlag {
			cmakeArgs = append(cmakeArgs, "-DALLOW_XP=ON")
		} else {
			cmakeArgs = append(cmakeArgs, "-DALLOW_XP=OFF")
		}
		// Some compilers are inherently cross-compilers, such as Clang and the QNX QCC compiler.
		// The CMAKE_<LANG>_COMPILER_TARGET can be set to pass a value to those supported compilers when compiling.
		// see https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_TARGET.html
		targetTriple := getLLVMTargetTripleMSVC(msvcTargetArchFlag)
		if strings.Contains(os.Getenv("CC"), "clang-cl") {
			cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_C_COMPILER_TARGET=%s", targetTriple))
			cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_CXX_COMPILER_TARGET=%s", targetTriple))
		}
		if msvcTargetArchFlag == "arm" || msvcTargetArchFlag == "arm64" {
			cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_ASM_FLAGS=--target=%s", targetTriple))
		}
	}

	if systemNameFlag == "darwin" {
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_OSX_DEPLOYMENT_TARGET=%s", macosxVersionMinFlag))
		if macosxUniversalBuildFlag {
			cmakeArgs = append(cmakeArgs, "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
		}
	}

	if systemNameFlag == "android" {
		androidAbiTarget, androidAppAbi = getAndroidTargetAndAppAbi(archFlag)
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_TOOLCHAIN_FILE=%s/../cmake/platforms/Android.cmake", buildDir))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DANDROID_ABI=%s", androidAppAbi))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DANDROID_ABI_TARGET=%s", androidAbiTarget))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DANDROID_API_VERSION=%d", androidApiLevel))

		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DLLVM_SYSROOT=%s/../third_party/llvm-build/Release+Asserts", buildDir))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_SYSTEM_PROCESSOR=%s", androidAppAbi))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_TARGET=%s%d", androidAbiTarget, androidApiLevel))
		// FIXME patch llvm toolchain to find libunwind.a
		getAndFixLibunwind();
	}

	if systemNameFlag == "linux" && sysrootFlag != "" {
		subsystem := subSystemNameFlag
		if subSystemNameFlag == "openwrt" {
			subsystem = "musl"
		}
		gnuType, gnuArch := getGNUTargetTypeAndArch(archFlag, subsystem)
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_TOOLCHAIN_FILE=%s/../cmake/platforms/Linux.cmake", buildDir))
		var pkgConfigPath = filepath.Join(sysrootFlag, "usr", "lib", "pkgconfig")
		pkgConfigPath += ";" + filepath.Join(sysrootFlag, "usr", "share", "pkgconfig")
		if err := os.Setenv("PKG_CONFIG_PATH", pkgConfigPath); err != nil {
			glog.Fatalf("%v", err)
		}
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DLLVM_SYSROOT=%s/../third_party/llvm-build/Release+Asserts", buildDir))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_SYSROOT=%s", sysrootFlag))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_SYSTEM_PROCESSOR=%s", gnuArch))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_TARGET=%s", gnuType))
		if subsystem == "" {
			cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DUSE_TCMALLOC=on"))
		}
	}

	if systemNameFlag == "freebsd" && sysrootFlag != "" {
		var llvmTarget string
		var llvmArch string

		if archFlag == "amd64" || archFlag == "x86_64" {
			llvmTarget = "x86_64-freebsd"
			llvmArch = "x86_64"
		} else if archFlag == "x86" || archFlag == "i386" {
			llvmTarget = "i386-freebsd"
			llvmArch = "i386"
		} else if archFlag == "aarch64" || archFlag == "arm64" {
			llvmTarget = "aarch64-freebsd"
			llvmArch = "aarch64"
		} else {
			glog.Fatalf("Invalid arch: %s", archFlag)
		}
		llvmTarget = fmt.Sprintf("%s%d", llvmTarget, freebsdAbiFlag)
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_TOOLCHAIN_FILE=%s/../cmake/platforms/FreeBSD.cmake", buildDir))
		var pkgConfigPath = filepath.Join(sysrootFlag, "usr", "libdata", "pkgconfig")
		pkgConfigPath += ";" + filepath.Join(sysrootFlag, "usr", "local", "libdata", "pkgconfig")
		if err := os.Setenv("PKG_CONFIG_PATH", pkgConfigPath); err != nil {
			glog.Fatalf("%v", err)
		}
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DLLVM_SYSROOT=%s/../third_party/llvm-build/Release+Asserts", buildDir))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_SYSROOT=%s", sysrootFlag))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_SYSTEM_PROCESSOR=%s", llvmArch))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_TARGET=%s", llvmTarget))
	}
	cmakeCmd := append([]string{"cmake", ".."}, cmakeArgs...)
	cmdRun(cmakeCmd, true)
}

func renameByUnlink(src string, dst string) error {
	if _, err := os.Stat(dst); err == nil {
		err = os.RemoveAll(dst)
		if err != nil {
			return err
		}
	}

	return os.Rename(src, dst)
}

func buildStageExecuteBuildScript() {
	glog.Info("BuildStage -- Execute Build Script")
	glog.Info("======================================================================")
	ninjaCmd := []string{"ninja", "-j", fmt.Sprintf("%d", cmakeBuildConcurrencyFlag), APPNAME}
	cmdRun(ninjaCmd, true)
	if buildBenchmarkFlag || runBenchmarkFlag {
		ninjaCmd := []string{"ninja", "-j", fmt.Sprintf("%d", cmakeBuildConcurrencyFlag), "yass_benchmark"}
		cmdRun(ninjaCmd, true)
	}
	if runBenchmarkFlag {
		benchmarkCmd := []string{"./yass_benchmark"}
		if verboseFlag > 0 {
			benchmarkCmd = []string{"./yass_benchmark", "-v", fmt.Sprintf("%d", verboseFlag), "-logtostderr"}
		}
		cmdRun(benchmarkCmd, true)
	}
	if buildTestFlag || runTestFlag {
		ninjaCmd := []string{"ninja", "-j", fmt.Sprintf("%d", cmakeBuildConcurrencyFlag), "yass_test"}
		cmdRun(ninjaCmd, true)
	}
	if runTestFlag {
		checkCmd := []string{"./yass_test"}
		if verboseFlag > 0 {
			checkCmd = []string{"./yass_test", "-v", fmt.Sprintf("%d", verboseFlag), "-logtostderr"}
		}
		cmdRun(checkCmd, true)
	}
	// FIXME move to cmake (required by Xcode generator)
	if systemNameFlag == "darwin" {
		if _, err := os.Stat(filepath.Join(cmakeBuildTypeFlag, getAppName())); err == nil {
			err := renameByUnlink(filepath.Join(cmakeBuildTypeFlag, getAppName()), getAppName())
			glog.Fatalf("%v", err)
		}
	}
}

func postStateCopyDependedLibraries() {
	glog.Info("PostState -- Copy Depended Libraries")
	glog.Info("======================================================================")
	// TBD
}

func postStateFixRPath() {
	glog.Info("PostState -- Fix RPATH")
	glog.Info("======================================================================")
	// TBD
}

func postStateStripBinaries() {
	glog.Info("PostState -- Strip Binaries")
	glog.Info("======================================================================")
	if systemNameFlag == "windows" {
		return
	}
	if systemNameFlag == "android" || systemNameFlag == "linux" || systemNameFlag == "freebsd" {
		objcopy := filepath.Join("..", "third_party", "llvm-build", "Release+Asserts", "bin", "llvm-objcopy")
		if _, err := os.Stat(objcopy); errors.Is(err, os.ErrNotExist) {
			objcopy = "objcopy"
		}
		// create a file containing the debugging info.
		cmdRun([]string{objcopy, "--only-keep-debug", APPNAME, APPNAME + ".dbg"}, false)
		// stripped executable.
		cmdRun([]string{objcopy, "--strip-debug", APPNAME}, false)
		// to add a link to the debugging info into the stripped executable.
		cmdRun([]string{objcopy, "--add-gnu-debuglink=" + APPNAME + ".dbg", APPNAME}, false)
	} else if systemNameFlag == "darwin" {
		cmdRun([]string{"dsymutil", filepath.Join(getAppName(), "Contents", "MacOS", APPNAME),
			"--statistics", "--papertrail", "-o", getAppName() + ".dSYM"}, false)
		cmdRun([]string{"strip", "-S", "-x", "-v", filepath.Join(getAppName(), "Contents", "MacOS", APPNAME)}, false)
	} else {
		glog.Warningf("not supported in platform %s", systemNameFlag)
	}
}

func postStateCodeSign() {
	glog.Info("PostState -- Code Sign")
	glog.Info("======================================================================")
	if cmakeBuildTypeFlag != "Release" || systemNameFlag != "darwin" {
		return
	}

	// reference https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution/resolving_common_notarization_issues?language=objc
	// Hardened runtime is available in the Capabilities pane of Xcode 10 or later
	//
	cmdRun([]string{"codesign", "--timestamp=none", "--preserve-metadata=entitlements", "--options=runtime", "--force", "--deep",
		"--sign", macosxCodeSignIdentityFlag, getAppName()}, true)
	cmdRun([]string{"codesign", "-dv", "--deep", "--strict", "--verbose=4",
		getAppName()}, true)
	cmdRun([]string{"codesign", "-d", "--entitlements", ":-", getAppName()}, true)
}

// Main returns the file name excluding extension.
func Main(path string) string {
	path = filepath.Base(path)
	for i := len(path) - 1; i >= 0 && !os.IsPathSeparator(path[i]); i-- {
		if path[i] == '.' {
			return path[:i]
		}
	}
	return path
}

func checkUniversalBuildFatBundle(bundlePath string) bool {
	mainFile := filepath.Join(bundlePath, "Contents", "MacOS", Main(bundlePath))
	// TODO should we check recursilvely?
	return checkUniversalBuildFatBinary(mainFile)
}

func checkUniversalBuildFatBinary(binaryFile string) bool {
	output := cmdCheckOutput([]string{"lipo", "-archs", binaryFile})
	return strings.Contains(output, "x86_64") && strings.Contains(output, "arm64")
}

func checkUniveralBuild(path string) error {
	checked := true
	statusMsg := ""
	errorMsg := ""
	if strings.HasSuffix(path, ".framework") || strings.HasSuffix(path, ".app") || strings.HasSuffix(path, ".appex") {
		if !checkUniversalBuildFatBundle(path) {
			statusMsg += fmt.Sprintf("bundle %s is not universal built\n", path)
			errorMsg += fmt.Sprintf("bundle %s is not universal built\n", path)
			checked = false
		} else {
			statusMsg += fmt.Sprintf("bundle %s is universal built\n", path)
		}

		walkFunc := func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return err
			}
			if info.IsDir() {
				if strings.HasSuffix(path, ".framework") || strings.HasSuffix(path, ".app") || strings.HasSuffix(path, ".appex") {
					if !checkUniversalBuildFatBundle(path) {
						statusMsg += fmt.Sprintf("bundle %s is not universal built\n", path)
						errorMsg += fmt.Sprintf("bundle %s is not universal built\n", path)
						checked = false
					} else {
						statusMsg += fmt.Sprintf("bundle %s is universal built\n", path)
					}
				}
			} else {
				if strings.HasSuffix(path, ".so") || strings.HasSuffix(path, ".dylib") {
					if !checkUniversalBuildFatBinary(path) {
						statusMsg += fmt.Sprintf("dylib %s is not universal built\n", path)
						errorMsg += fmt.Sprintf("dylib %s is not universal built\n", path)
						checked = false
					} else {
						statusMsg += fmt.Sprintf("dylib %s is universal built\n", path)
					}
				}
			}
			return nil
		}
		err := filepath.Walk(path, walkFunc)
		if err != nil {
			return err
		}
		glog.V(2).Info(statusMsg)
	}
	if checked {
		return nil
	}
	return errors.New(errorMsg)
}

func postCheckUniversalBuild() {
	glog.Info("PostState -- Check Universal Build")
	glog.Info("======================================================================")
	if systemNameFlag == "darwin" && macosxUniversalBuildFlag {
		err := checkUniveralBuild(getAppName())
		if err != nil {
			glog.Fatalf("%v", err)
		}
	}
}

func copyFile(src string, dst string) error {
	buf := make([]byte, 4096)

	fin, err := os.Open(src)
	if err != nil {
		return err
	}

	defer func(fin *os.File) {
		err := fin.Close()
		if err != nil {
			glog.Fatalf("%v", err)
		}
	}(fin)

	fout, err := os.Create(dst)
	if err != nil {
		return err
	}

	defer func(fout *os.File) {
		err := fout.Close()
		if err != nil {
			glog.Fatalf("%v", err)
		}
	}(fout)

	for {

		n, err := fin.Read(buf)
		if err != nil && err != io.EOF {
			return err
		}

		if n == 0 {
			break
		}

		if _, err := fout.Write(buf[:n]); err != nil {
			return err
		}
	}
	return nil
}

// LICENSEs
func postStateArchiveLicenses() []string {
	licenseMaps := map[string]string{
		"LICENSE":            filepath.Join("..", "LICENSE"),
		"LICENSE.abseil-cpp": filepath.Join("..", "third_party", "abseil-cpp", "LICENSE"),
		"LICENSE.asio":       filepath.Join("..", "third_party", "asio", "asio", "LICENSE_1_0.txt"),
		"LICENSE.boringssl":  filepath.Join("..", "third_party", "boringssl", "src", "LICENSE"),
		"LICENSE.googleurl":  filepath.Join("..", "third_party", "googleurl", "LICENSE"),
		"LICENSE.icu":        filepath.Join("..", "third_party", "icu", "LICENSE"),
		"LICENSE.json":       filepath.Join("..", "third_party", "json", "LICENSE.MIT"),
		"LICENSE.leveldb":    filepath.Join("..", "third_party", "leveldb", "LICENSE"),
		"LICENSE.libc++":     filepath.Join("..", "third_party", "libc++", "trunk", "LICENSE.TXT"),
		"LICENSE.libc++abi":  filepath.Join("..", "third_party", "libc++abi", "trunk", "LICENSE.TXT"),
		"LICENSE.lss":        filepath.Join("..", "third_party", "lss", "LICENSE"),
		"LICENSE.mbedtls":    filepath.Join("..", "third_party", "mbedtls", "LICENSE"),
		"LICENSE.mozilla":    filepath.Join("..", "third_party", "url", "third_party", "mozilla", "LICENSE.txt"),
		"LICENSE.protobuf":   filepath.Join("..", "third_party", "protobuf", "LICENSE"),
		"LICENSE.quiche":     filepath.Join("..", "third_party", "quiche", "src", "LICENSE"),
		"LICENSE.tcmalloc":   filepath.Join("..", "third_party", "tcmalloc", "src", "LICENSE"),
		"LICENSE.xxhash":     filepath.Join("..", "third_party", "xxhash", "LICENSE"),
		"LICENSE.zlib":       filepath.Join("..", "third_party", "zlib", "LICENSE"),
	}
	var licenses []string
	for license := range licenseMaps {
		if err := copyFile(licenseMaps[license], license); err != nil {
			glog.Fatalf("%v", err)
		}
		licenses = append(licenses, license)
	}
	return licenses
}

// add to zip writer
func archiveFileToZip(zipWriter *zip.Writer, info os.FileInfo, prefix string, path string) error {
	if info.Mode()&os.ModeSymlink == os.ModeSymlink {
		symlinksTarget, err := os.Readlink(path)
		if err != nil {
			return err
		}
		info, err := os.Lstat(path)
		if err != nil {
			return err
		}
		header, err := zip.FileInfoHeader(info)
		if err != nil {
			return err
		}
		symlinksTarget = filepath.Join(prefix, symlinksTarget)
		header.Name = filepath.Join(prefix, header.Name)
		header.Method = zip.Deflate
		writer, err := zipWriter.CreateHeader(header)
		if err != nil {
			return err
		}

		// Write symlink's target to writer - file's body for symlinks is the symlink target.
		_, err = writer.Write([]byte(filepath.ToSlash(symlinksTarget)))
		if err != nil {
			return err
		}
		return nil
	}
	f, err := os.Open(path)
	if err != nil {
		return err
	}
	defer func(f *os.File) {
		err := f.Close()
		if err != nil {
			glog.Fatalf("%v", err)
		}
	}(f)
	w, err := zipWriter.Create(filepath.Join(prefix, path))
	if err != nil {
		return err
	}
	if _, err := io.Copy(w, f); err != nil {
		return err
	}
	return nil
}

// generate tgz (posix) or zip file
func archiveFiles(output string, prefix string, paths []string) {
	if strings.HasSuffix(output, ".tgz") {
		glog.Infof("generating tgz file %s", output)
		// use below if tar supports gnu style
		// cmd := []string{"tar", "caf", output, "--xform", fmt.Sprintf("s,^,%s/,", prefix)}
		cmd := []string{"mkdir", prefix}
		cmdRun(cmd, true)
		cmd = []string{"cp", "-rf"}
		cmd = append(cmd, paths...)
		cmd = append(cmd, prefix)
		cmdRun(cmd, true)
		cmd = []string{"tar", "cfz", output, prefix}
		cmdRun(cmd, true)
		cmd = []string{"rm", "-rf", prefix}
		cmdRun(cmd, true)
		return
	}
	glog.Infof("generating zip file %s", output)
	archive, err := os.Create(output)
	if err != nil {
		glog.Fatalf("%v", err)
	}
	defer func(archive *os.File) {
		err := archive.Close()
		if err != nil {
			glog.Fatalf("%v", err)
		}
	}(archive)
	zipWriter := zip.NewWriter(archive)
	for _, path := range paths {
		info, err := os.Stat(path)
		if err != nil {
			glog.Fatalf("%v", err)
		}
		if !info.IsDir() {
			err := archiveFileToZip(zipWriter, info, prefix, path)
			if err != nil {
				glog.Fatalf("%v", err)
			}
			continue
		}

		walkFunc := func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return err
			}
			if !info.IsDir() {
				err := archiveFileToZip(zipWriter, info, prefix, path)
				if err != nil {
					return err
				}
			}
			return nil
		}
		err = filepath.Walk(path, walkFunc)
		if err != nil {
			glog.Fatalf("%v", err)
		}
	}
	err = zipWriter.Close()
	if err != nil {
		glog.Fatalf("%v", err)
	}
}

func archiveMainFile(output string, prefix string, paths []string) {
	if systemNameFlag == "darwin" {
		var eulaRtf []byte
		eulaTemplate, err := ioutil.ReadFile("../src/mac/eula.xml")
		if err != nil {
			glog.Fatalf("%v", err)
		}
		eulaRtf, err = ioutil.ReadFile("../GPL-2.0.rtf")
		if err != nil {
			glog.Fatalf("%v", err)
		}

		eulaXml := strings.Replace(string(eulaTemplate), "%PLACEHOLDER%", base64.StdEncoding.EncodeToString([]byte(eulaRtf)), -1)

		err = ioutil.WriteFile("eula.xml", []byte(eulaXml), 0666)
		if err != nil {
			glog.Fatalf("%v", err)
		}

		// Use this command line to update .DS_Store
		// hdiutil convert -format UDRW -o yass.dmg yass-macos-release-universal-*.dmg
		// hdiutil resize -size 1G yass.dmg
		cmdCheckOutput([]string{"../scripts/pkg-dmg",
			"--source", paths[0],
			"--target", output,
			"--sourcefile",
			"--volname", "Yet Another Shadow Socket",
			"--resource", "eula.xml",
			"--icon", "../src/mac/yass.icns",
			"--copy", "../macos/.DS_Store:/.DS_Store",
			"--copy", "../macos/.background:/",
			"--symlink", "/Applications:/Applications"})
	} else {
		archiveFiles(output, prefix, paths)
	}
}

func generateMsi(output string, dllPaths []string, licensePaths []string) {
	wxsTemplate, err := ioutil.ReadFile(filepath.Join("..", "yass.wxs"))
	if err != nil {
		glog.Fatalf("%v", err)
	}
	wxsXml := string(wxsTemplate)
	guidReplacement := strings.ToUpper(uuid.New().String())
	wxsXml = strings.Replace(wxsXml, "YOURGUID", guidReplacement, 1)
	dllReplacement := ""
	for _, dllPath := range dllPaths {
		dllReplacement += fmt.Sprintf("<File Name='%s' Source='%s' KeyPath='no' />\n", filepath.Base(dllPath), dllPath)
	}
	wxsXml = strings.Replace(wxsXml, "<!-- %DLLPLACEHOLDER% -->", dllReplacement, 1)
	licenseReplacement := ""
	for _, licensePath := range licensePaths {
		licenseReplacement += fmt.Sprintf("<File Name='%s' Source='%s' KeyPath='no' />\n", filepath.Base(licensePath), licensePath)
	}
	wxsXml = strings.Replace(wxsXml, "<!-- %LICENSEPLACEHOLDER% -->", licenseReplacement, 1)

	err = ioutil.WriteFile("yass.wxs", []byte(wxsXml), 0666)
	if err != nil {
		glog.Fatalf("%v", err)
	}

	glog.Info("Feeding WiX compiler...")
	cmdRun([]string{"candle.exe", "yass.wxs", "-dPlatform=" + msvcTargetArchFlag, "-dVersion=" + tagFlag}, true)

	glog.Info("Generating MSI file...")
	cmdRun([]string{"light.exe", "-ext", "WixUIExtension", "-out", output, "-cultures:en-US", "-sice:ICE03", "-sice:ICE57", "-sice:ICE61", "yass.wixobj"}, true)
}

func generateNSIS(output string) {
	nsiTemplate, err := ioutil.ReadFile(filepath.Join("..", "yass.nsi"))
	if err != nil {
		glog.Fatalf("%v", err)
	}
	nsiContent := string(nsiTemplate)
	nsiContent = strings.Replace(nsiContent, "yass-installer.exe", output, 1)
	err = ioutil.WriteFile("yass.nsi", []byte(nsiContent), 0666)
	if err != nil {
		glog.Fatalf("%v", err)
	}
	glog.Info("Feeding NSIS compiler...")
	cmdRun([]string{"C:\\Program Files (x86)\\NSIS\\makensis.exe", "/XSetCompressor /FINAL lzma", "yass.nsi"}, true)
}

func generateOpenWrtMakefile(archive string, pkg_version string) {
	archive_dir, _ := filepath.Abs("..")
	archive_dir += "/"
	archive_checksum := strings.TrimSpace(cmdCheckOutput([]string{"sha256sum", archive}))
	archive_checksum = strings.Split(archive_checksum, " ")[0]
	mkTemplate, err := ioutil.ReadFile(filepath.Join("..", "openwrt", "Makefile.tmpl"))
	if err != nil {
		glog.Fatalf("%v", err)
	}
	mkContent := string(mkTemplate)
	mkContent = strings.Replace(mkContent, "%%PKG_VERSION%%", pkg_version, 1)
	mkContent = strings.Replace(mkContent, "%%PKG_DIR%%", archive_dir, 1)
	mkContent = strings.Replace(mkContent, "%%PKG_SHA256SUM%%", archive_checksum, 1)
	err = ioutil.WriteFile(filepath.Join("..", "openwrt", "Makefile"), []byte(mkContent), 0666)
	if err != nil {
		glog.Fatalf("%v", err)
	}
	glog.Info("OpenWRT Makefile written to openwrt/Makefile")
}

func postStateArchives() map[string][]string {
	glog.Info("PostState -- Archives")
	glog.Info("======================================================================")
	tag := fullTagFlag
	var archiveFormat string
	if systemNameFlag == "windows" {
		osName := "win"
		if msvcAllowXpFlag {
			osName = "winxp"
		}
		archiveFormat = fmt.Sprintf("%%s-%s-release-%s-%s-%s%%s%%s", osName, msvcTargetArchFlag, msvcCrtLinkageFlag, tag)
	} else if systemNameFlag == "darwin" {
		osName := "macos"
		arch := archFlag
		if macosxUniversalBuildFlag {
			arch = "universal"
		}
		archiveFormat = fmt.Sprintf("%%s-%s-release-%s-%s%%s%%s", osName, arch, tag)
	} else if systemNameFlag == "freebsd" {
		archiveFormat = fmt.Sprintf("%%s-%s%d-release-%s-%s%%s%%s", systemNameFlag, freebsdAbiFlag, archFlag, tag)
	} else {
		archiveFormat = fmt.Sprintf("%%s-%s-release-%s-%s%%s%%s", systemNameFlag, archFlag, tag)
		if subSystemNameFlag != "" {
			archiveFormat = fmt.Sprintf("%%s-%s-%s-release-%s-%s%%s%%s", systemNameFlag, subSystemNameFlag, archFlag, tag)
		}
	}

	ext := ".zip"

	if systemNameFlag == "android" || systemNameFlag == "linux" || systemNameFlag == "freebsd" {
		ext = ".tgz"
	}

	archive := fmt.Sprintf(archiveFormat, APPNAME, "", ext)
	archivePrefix := fmt.Sprintf(archiveFormat, strings.Replace(APPNAME, "_", "-", 1), "", "")
	archiveSuffix := fmt.Sprintf(archiveFormat, "", "", "")
	archiveSuffix = archiveSuffix[1:]
	if systemNameFlag == "darwin" {
		archive = fmt.Sprintf(archiveFormat, APPNAME, "", ".dmg")
	}

	msiArchive := fmt.Sprintf(archiveFormat, APPNAME, "", ".msi")
	nsisArchive := fmt.Sprintf(archiveFormat, APPNAME, "-installer", ".exe")
	debugArchive := fmt.Sprintf(archiveFormat, APPNAME, "-debuginfo", ext)

	archive = filepath.Join("..", archive)
	msiArchive = filepath.Join("..", msiArchive)
	nsisArchive = filepath.Join("..", nsisArchive)
	debugArchive = filepath.Join("..", debugArchive)

	archives := map[string][]string{}

	paths := []string{getAppName()}
	var dllPaths []string
	var dbgPaths []string

	// copying dependent LICENSEs
	licensePaths := postStateArchiveLicenses()
	paths = append(paths, licensePaths...)

	// main bundle
	archiveMainFile(archive, archivePrefix, paths)
	archives[archive] = paths

	// msi installer
	// FIXME wixtoolset3.14 supports arm64 but only 3.11 is out for release
	// https://github.com/wixtoolset/issues/issues/5558
	// error CNDL0265 : The Platform attribute has an invalid value arm64.
	// Possible values are x86, x64, or ia64.
	if systemNameFlag == "windows" && msvcTargetArchFlag != "arm64" {
		generateMsi(msiArchive, dllPaths, licensePaths)
		archives[msiArchive] = []string{msiArchive}
	}
	// nsis installer
	if systemNameFlag == "windows" && msvcTargetArchFlag != "arm64" {
		generateNSIS(nsisArchive)
		archives[nsisArchive] = []string{nsisArchive}
	}
	// debuginfo file
	if systemNameFlag == "windows" {
		archiveFiles(debugArchive, archivePrefix, []string{APPNAME + ".pdb"})
		dbgPaths = append(dbgPaths, APPNAME+".pdb")
	} else if systemNameFlag == "android" || systemNameFlag == "linux" || systemNameFlag == "freebsd" {
		archiveFiles(debugArchive, archivePrefix, []string{APPNAME + ".dbg"})
		dbgPaths = append(dbgPaths, APPNAME+".dbg")
	} else if systemNameFlag == "darwin" {
		archiveFiles(debugArchive, archivePrefix, []string{getAppName() + ".dSYM"})
		dbgPaths = append(dbgPaths, getAppName()+".dSYM")
	}
	archives[debugArchive] = dbgPaths

	// Create openwrt Makefile
	if subSystemNameFlag == "openwrt" && variantFlag == "cli" {
		generateOpenWrtMakefile(archive, archiveSuffix)
	}
	return archives
}

func get7zPath() string {
	if systemNameFlag == "windows" {
		return "C:\\Program Files\\7-Zip\\7z.exe"
	} else {
		return "7z"
	}
}

func inspectArchive(file string, files []string) {
	if strings.HasSuffix(file, ".dmg") {
		cmdRun([]string{"hdiutil", "imageinfo", file}, false)
	} else if strings.HasSuffix(file, ".zip") || strings.HasSuffix(file, ".msi") || strings.HasSuffix(file, ".exe") {
		p7z := get7zPath()
		cmdRun([]string{p7z, "l", file}, false)
	} else if strings.HasSuffix(file, ".tgz") {
		cmdRun([]string{"tar", "tvf", file}, false)
	} else {
		for _, file := range files {
			glog.Infof("------------ %s", file)
		}
	}
}

func postStateInspectArchives(archives map[string][]string) {
	glog.Info("PostState -- Inspect Archives")
	glog.Info("======================================================================")
	for archive := range archives {
		glog.Infof("------ %s:", filepath.Base(archive))
		glog.Infof("======================================================================")
		inspectArchive(archive, archives[archive])
	}
}

func main() {
	InitFlag()
	// PreStage Find Source Directory
	prebuildFindSourceDirectory()
	// BuildStage Generate Build Script
	buildStageGenerateBuildScript()
	// BuildStage Execute Build Script
	buildStageExecuteBuildScript()
	if noPackagingFlag {
		return
	}
	// PostStage Copy depended libraries
	postStateCopyDependedLibraries()
	// PostStage Fix RPath
	postStateFixRPath()
	// PostStage Strip Binaries
	postStateStripBinaries()
	// PostStage Code Sign
	postStateCodeSign()
	// PostStage Check Universal Build
	postCheckUniversalBuild()
	// PostStage Archive
	archives := postStateArchives()
	// PostStage Inspect Archive
	postStateInspectArchives(archives)
}
