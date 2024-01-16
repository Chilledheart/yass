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
	"reflect"
	"regexp"

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
var noBuildFlag bool
var noConfigureFlag bool
var noPackagingFlag bool
var buildBenchmarkFlag bool
var runBenchmarkFlag bool
var buildTestFlag bool
var runTestFlag bool
var verboseFlag int

var cmakeBuildTypeFlag string
var cmakeBuildConcurrencyFlag int

var clangPath string
var useLibCxxFlag bool
var enableLtoFlag bool
var useIcuFlag bool

var clangTidyModeFlag bool
var clangTidyExecutablePathFlag string

var macosxVersionMinFlag string
var macosxUniversalBuildFlag bool
var macosxKeychainPathFlag string
var macosxCodeSignIdentityFlag string

var iosVersionMinFlag string
var iosCodeSignIdentityFlag string
var iosDevelopmentTeamFlag string
var iosTestDeviceNameFlag string

var msvcTargetArchFlag string
var msvcCrtLinkageFlag string
var msvcAllowXpFlag bool

var freebsdAbiFlag int

var systemNameFlag string
var subSystemNameFlag string
var sysrootFlag string
var archFlag string

var variantFlag string

var mingwDir string
var mingwAllowXpFlag bool

var androidAppAbi string
var androidAbiTarget string
var androidApiLevel int

var androidSdkDir string
var androidNdkDir string

var harmonyAppAbi string
var harmonyAbiTarget string

var harmonyNdkDir string

func getAppName() string {
	if systemNameFlag == "windows" {
		return APPNAME + ".exe"
	} else if systemNameFlag == "darwin" || systemNameFlag == "ios" {
		return APPNAME + ".app"
	} else if systemNameFlag == "mingw" {
		return APPNAME + ".exe"
	} else if systemNameFlag == "android" {
		if APPNAME == "yass" {
			return "lib" + APPNAME + ".so"
		}
		return APPNAME
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
	flag.BoolVar(&noBuildFlag, "no-build", false, "Skip build step")
	flag.BoolVar(&noConfigureFlag, "no-configure", false, "Skip configure step")
	flag.BoolVar(&noPackagingFlag, "no-packaging", false, "Skip packaging step")
	flag.BoolVar(&buildBenchmarkFlag, "build-benchmark", false, "Build benchmarks")
	flag.BoolVar(&runBenchmarkFlag, "run-benchmark", false, "Build and run benchmarks")
	flag.BoolVar(&buildTestFlag, "build-test", false, "Build unittests")
	flag.BoolVar(&runTestFlag, "run-test", false, "Build and run unittests")
	flag.IntVar(&verboseFlag, "verbose", 0, "Run unittests with verbose level")

	flag.StringVar(&cmakeBuildTypeFlag, "cmake-build-type", getEnv("BUILD_TYPE", "Release"), "Set cmake build configuration")
	flag.IntVar(&cmakeBuildConcurrencyFlag, "cmake-build-concurrency", runtime.NumCPU(), "Set cmake build concurrency")

	builtClangPath, _ := filepath.Abs(filepath.Join(projectDir, "third_party", "llvm-build", "Release+Asserts"))
	flag.StringVar(&clangPath, "clang-path", builtClangPath, "Compiler clang's path")

	flag.BoolVar(&useLibCxxFlag, "use-libcxx", true, "Use Custom libc++")
	flag.BoolVar(&enableLtoFlag, "enable-lto", true, "Enable lto")
	flag.BoolVar(&useIcuFlag, "use-icu", false, "Use ICU Feature")

	flag.BoolVar(&clangTidyModeFlag, "clang-tidy-mode", getEnvBool("ENABLE_CLANG_TIDY", false), "Enable Clang Tidy Build")
	flag.StringVar(&clangTidyExecutablePathFlag, "clang-tidy-executable-path", getEnv("CLANG_TIDY_EXECUTABLE", ""), "Path to clang-tidy, only used by Clang Tidy Build")

	flag.StringVar(&macosxVersionMinFlag, "macosx-version-min", getEnv("MACOSX_DEPLOYMENT_TARGET", "10.14"), "Set Mac OS X deployment target, such as 10.15")
	flag.BoolVar(&macosxUniversalBuildFlag, "macosx-universal-build", getEnvBool("ENABLE_OSX_UNIVERSAL_BUILD", false), "Enable Mac OS X Universal Build")
	flag.StringVar(&macosxKeychainPathFlag, "macosx-keychain-path", getEnv("KEYCHAIN_PATH", ""), "During signing, only search for the signing identity in the keychain file specified")
	flag.StringVar(&macosxCodeSignIdentityFlag, "macosx-codesign-identity", getEnv("CODESIGN_IDENTITY", "-"), "Set Mac OS X CodeSign Identity")

	flag.StringVar(&iosVersionMinFlag, "ios-version-min", getEnv("MACOSX_DEPLOYMENT_TARGET", "13.0"), "Set iOS deployment target, such as 13.0")
	flag.StringVar(&iosCodeSignIdentityFlag, "ios-codesign-identity", getEnv("CODESIGN_IDENTITY", "-"), "Set iOS CodeSign Identity")
	flag.StringVar(&iosDevelopmentTeamFlag, "ios-development-team", getEnv("DEVELOPMENT_TEAM", ""), "Set iOS deployment team")
	flag.StringVar(&iosTestDeviceNameFlag, "ios-test-device-name", getEnv("IPHONE_NAME", ""), "Set iOS test device name")

	flag.StringVar(&msvcTargetArchFlag, "msvc-tgt-arch", getEnv("VSCMD_ARG_TGT_ARCH", "x64"), "Set Visual C++ Target Achitecture")
	flag.StringVar(&msvcCrtLinkageFlag, "msvc-crt-linkage", getEnv("MSVC_CRT_LINKAGE", "dynamic"), "Set Visual C++ CRT Linkage")
	flag.BoolVar(&msvcAllowXpFlag, "msvc-allow-xp", getEnvBool("MSVC_ALLOW_XP", false), "Enable Windows XP Build")

	flag.IntVar(&freebsdAbiFlag, "freebsd-abi", getFreebsdABI(11), "Select FreeBSD ABI")

	flag.StringVar(&systemNameFlag, "system", runtime.GOOS, "Specify host system name")
	flag.StringVar(&subSystemNameFlag, "subsystem", "", "Specify host subsystem name")
	flag.StringVar(&sysrootFlag, "sysroot", "", "Specify host sysroot, used in cross-compiling")
	flag.StringVar(&archFlag, "arch", runtime.GOARCH, "Specify host architecture")

	flag.StringVar(&variantFlag, "variant", "gui", "Specify variant, available: gui, cli, server")

	flag.StringVar(&mingwDir, "mingw-dir", "", "MinGW Dir Path")
	flag.BoolVar(&mingwAllowXpFlag, "mingw-allow-xp", false, "Enable Windows XP Build")

	flag.IntVar(&androidApiLevel, "android-api", 24, "Select Android API Level")
	flag.StringVar(&androidSdkDir, "android-sdk-dir", getEnv("ANDROID_SDK_ROOT", ""), "Android SDK Home Path")
	flag.StringVar(&androidNdkDir, "android-ndk-dir", getEnv("ANDROID_NDK_ROOT", "../third_party/android_toolchain"), "Android NDK Home Path")

	flag.StringVar(&harmonyNdkDir, "harmony-ndk-dir", getEnv("HARMONY_NDK_ROOT", ""), "OpenHarmony NDK Home Path")

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
	} else if systemNameFlag == "mingw" {
		osSuffix := ""
		if mingwAllowXpFlag {
			osSuffix = "-winxp"
		}
		buildDir = fmt.Sprintf("build-%s%s-%s", systemNameFlag, osSuffix, archFlag)
	} else if systemNameFlag == "freebsd" {
		buildDir = fmt.Sprintf("build-%s%d-%s", systemNameFlag, freebsdAbiFlag, archFlag)
	} else if systemNameFlag == "android" {
		buildDir = fmt.Sprintf("build-%s%d-%s", systemNameFlag, androidApiLevel, archFlag)
	} else if systemNameFlag == "ios" {
		buildDir = fmt.Sprintf("build-%s-%s", systemNameFlag, archFlag)
		if subSystemNameFlag != "" {
			buildDir = fmt.Sprintf("build-%s-%s-%s", systemNameFlag, subSystemNameFlag, archFlag)
		}
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

func getMinGWTargetAndAppAbi(arch string) (string, string) {
	if arch == "x86" || arch == "i686" {
		return "i686-w64-mingw32", "i686"
	} else if arch == "x64" || arch == "x86_64" || arch == "amd64" {
		return "x86_64-w64-mingw32", "x86_64"
	} else if arch == "arm64" || arch == "aarch64" {
		return "aarch64-w64-mingw32", "aarch64"
	}
	glog.Fatalf("Invalid MinGW arch: %s", arch)
	return "", ""
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

// ohos.toolchain.cmake
func getHarmonyTargetAndAppAbi(arch string) (string, string) {
	if arch == "x64" {
		return "x86_64-linux-ohos", "x86_64"
	} else if arch == "arm64" {
		return "aarch64-linux-ohos", "arm64-v8a"
	} else if arch == "arm" {
		// armeabi for armv6
		return "arm-linux-ohos", "armeabi-v7a"
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

func getLLVMVersion() string {
	entries, err := os.ReadDir(filepath.Join(clangPath, "lib", "clang"))
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
	return llvm_version
}

func getAndFixMinGWLibunwind(mingwDir string) {
	getAndFixLibunwind(fmt.Sprintf("%s/lib/clang/16/lib/windows", mingwDir), "windows")
}

func getAndFixAndroidLibunwind(ndkDir string) {
	getAndFixLibunwind(fmt.Sprintf("%s/toolchains/llvm/prebuilt/%s-x86_64/lib64/clang/14.0.7/lib/linux", ndkDir, runtime.GOOS), "linux")
}

func getAndFixHarmonyLibunwind() {
	// FIX Runtime
	getAndFixLibunwind(fmt.Sprintf("%s/native/llvm/lib/clang/12.0.1/lib", harmonyNdkDir), "")
	// FIX libunwind
	target_path := fmt.Sprintf(filepath.Join(clangPath, "lib"))
	source_path := fmt.Sprintf("%s/native/llvm/lib", harmonyNdkDir)
	entries, err := os.ReadDir(source_path)
	if err != nil {
		glog.Fatalf("%v", err)
	}
	for _, entry := range entries {
		if !strings.Contains(entry.Name(), "-ohos") {
			continue;
		}
		if _, err = os.Lstat(filepath.Join(target_path, entry.Name())); err == nil {
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

func getAndFixLibunwind(source_path string, subdir string) {
	if runtime.GOOS == "windows" {
		glog.Fatalf("Symbolic link is not supported on windows")
	}
	// ln -sf $PWD/third_party/android_toolchain/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/14.0.7/lib/linux/i386 third_party/llvm-build/Release+Asserts/lib/clang/18/lib/linux
	entries, err := os.ReadDir(filepath.Join(clangPath, "lib", "clang"))
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
	source_path, err = filepath.Abs(source_path)
	if err != nil {
		glog.Fatalf("%v", err)
	}
	target_path := fmt.Sprintf(filepath.Join(clangPath, "lib", "clang", llvm_version, "lib", subdir))
	entries, err = os.ReadDir(source_path)
	if err != nil {
		glog.Fatalf("%v", err)
	}
	if _, err = os.Stat(target_path); errors.Is(err, os.ErrNotExist) {
		err = os.Mkdir(target_path, 0777);
		if err != nil {
			glog.Fatalf("%v", err)
		}
	}
	for _, entry := range entries {
		if subdir == "" && entry.Name() == "linux" {
			continue;
		}
		if _, err = os.Lstat(filepath.Join(target_path, entry.Name())); err == nil {
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
	if (os.Getenv("CC") != "") {
		glog.Infof("Using overrided compiler %s", os.Getenv("CC"))
	} else if systemNameFlag == "ios" {
		glog.Infof("Using xcode's builtin compiler")
	} else if (clangPath != "") {
		if systemNameFlag == "windows" {
			_clangClPath := filepath.Join(clangPath, "bin", "clang-cl.exe")
			if _, err := os.Stat(_clangClPath); errors.Is(err, os.ErrNotExist) {
				glog.Fatalf("Compiler %s not found", _clangClPath)
			}
			os.Setenv("CC", _clangClPath)
			os.Setenv("CXX", _clangClPath)
			glog.Infof("Using compiler %s", _clangClPath)
		} else if runtime.GOOS == "windows" {
			_clangPath := filepath.Join(clangPath, "bin", "clang.exe")
			_clangCxxPath := filepath.Join(clangPath, "bin", "clang++.exe")

			if _, err := os.Stat(_clangPath); errors.Is(err, os.ErrNotExist) {
				glog.Fatalf("Compiler %s not found", _clangPath)
			}
			os.Setenv("CC", _clangPath)
			os.Setenv("CXX", _clangCxxPath)
			glog.Infof("Using compiler %s", _clangPath)
		} else {
			_clangPath := filepath.Join(clangPath, "bin", "clang")
			_clangCxxPath := filepath.Join(clangPath, "bin", "clang++")

			if _, err := os.Stat(_clangPath); errors.Is(err, os.ErrNotExist) {
				glog.Fatalf("Compiler %s not found", _clangPath)
			}
			os.Setenv("CC", _clangPath)
			os.Setenv("CXX", _clangCxxPath)
			glog.Infof("Using compiler %s", _clangPath)
		}
	}
	cmakeArgs = append(cmakeArgs, "-DGUI=ON", "-DCLI=ON", "-DSERVER=ON")
	cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_BUILD_TYPE=%s", cmakeBuildTypeFlag))
	if systemNameFlag == "ios" {
		cmakeArgs = append(cmakeArgs, "-G", "Xcode")
	} else {
		cmakeArgs = append(cmakeArgs, "-G", "Ninja")
	}
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
	if useIcuFlag {
		cmakeArgs = append(cmakeArgs, "-DUSE_ICU=on")
	} else {
		cmakeArgs = append(cmakeArgs, "-DUSE_ICU=off")
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
		} else {
			if archFlag == "x64" || archFlag == "x86_64" || archFlag == "amd64" {
				cmakeArgs = append(cmakeArgs, "-DCMAKE_SYSTEM_PROCESSOR=x86_64")
				cmakeArgs = append(cmakeArgs, "-DCMAKE_OSX_ARCHITECTURES=x86_64")
			} else if archFlag == "arm64" {
				cmakeArgs = append(cmakeArgs, "-DCMAKE_SYSTEM_PROCESSOR=arm64")
				cmakeArgs = append(cmakeArgs, "-DCMAKE_OSX_ARCHITECTURES=arm64")
			} else if archFlag == "" {
				// nop
			} else {
				glog.Fatalf("Invalid archFlag: %s", archFlag);
			}
		}
	}

	if systemNameFlag == "mingw" {
		if (mingwDir != "") {
			glog.Infof("Using llvm-mingw dir %s", mingwDir);
			glog.Infof("Using clang dir %s", clangPath);
		} else {
			mingwDir = clangPath
			glog.Infof("Using llvm-mingw dir %s", clangPath);
			glog.Infof("Using clang inside llvm-mingw dir %s", mingwDir);
		}
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCROSS_TOOLCHAIN_FLAGS_TOOLCHAIN_FILE=%s/Native.cmake", buildDir))

		ccCompiler := os.Getenv("CC")
		cxxCompiler := os.Getenv("CXX")
		nativeToolChainContent := strings.Replace(fmt.Sprintf("set(CMAKE_C_COMPILER \"%s\")\n", ccCompiler), "\\", "/", -1)
		nativeToolChainContent += strings.Replace(fmt.Sprintf("set(CMAKE_CXX_COMPILER \"%s\")\n", cxxCompiler), "\\", "/", -1)
		err := ioutil.WriteFile("Native.cmake", []byte(nativeToolChainContent), 0666)
		if err != nil {
			glog.Fatalf("%v", err)
		}

		targetTriple, targetAbi := getMinGWTargetAndAppAbi(archFlag)
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DHOST_OS=%s", runtime.GOOS))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_TOOLCHAIN_FILE=%s/../cmake/platforms/MinGW.cmake", buildDir))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DLLVM_SYSROOT=%s", clangPath))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DMINGW_SYSROOT=%s", mingwDir))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_SYSTEM_PROCESSOR=%s", targetAbi))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_TARGET=%s", targetTriple))

		if mingwAllowXpFlag {
			cmakeArgs = append(cmakeArgs, "-DALLOW_XP=ON")
			cmakeArgs = append(cmakeArgs, "-DMINGW_MSVCRT100=ON")
			cmakeArgs = append(cmakeArgs, "-DMINGW_WORKAROUND=ON")
			llvm_version := getLLVMVersion()
			clang_rt_suffix := targetAbi
			if (targetAbi == "i686") {
				clang_rt_suffix = "i386"
			}
			cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DMINGW_COMPILER_RT=%s/lib/clang/%s/lib/windows/libclang_rt.builtins-%s.a", clangPath, llvm_version, clang_rt_suffix))
		} else {
			cmakeArgs = append(cmakeArgs, "-DALLOW_XP=OFF")
		}

		if (mingwDir != clangPath) {
			getAndFixMinGWLibunwind(mingwDir)
		}
	}

	if systemNameFlag == "ios" {
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_TOOLCHAIN_FILE=%s/../cmake/platforms/ios.toolchain.cmake", buildDir))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DDEPLOYMENT_TARGET=%s", iosVersionMinFlag))
		// FIXME ios doens't support c-ares
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DUSE_CARES=%s", "OFF"))
		platform := "OS"
		if subSystemNameFlag == "simulator" {
			if archFlag == "x86" {
				platform = "SIMULATOR"
			} else if archFlag == "x64" || archFlag == "x86_64" || archFlag == "amd64" {
				platform = "SIMULATOR64"
			} else if archFlag == "arm64" {
				platform = "SIMULATORARM64"
			} else {
				glog.Fatalf("Invalid archFlag: %s", archFlag);
			}
			cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DXCODE_CODESIGN_IDENTITY=%s", iosCodeSignIdentityFlag))
			cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DXCODE_DEPLOYMENT_TEAM=%s", iosDevelopmentTeamFlag))
			glog.Info("No Packaging supported for simulator, disabling...")
			noPackagingFlag = true
		} else if subSystemNameFlag != "" {
			glog.Fatalf("Invalid subSystemNameFlag: %s", subSystemNameFlag);
		} else {
			if archFlag == "arm" {
				platform = "OS"
			} else if archFlag == "arm64" {
				platform = "OS64"
			} else {
				glog.Fatalf("Invalid archFlag: %s", archFlag);
			}
			cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DXCODE_CODESIGN_IDENTITY=%s", iosCodeSignIdentityFlag))
			cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DXCODE_DEPLOYMENT_TEAM=%s", iosDevelopmentTeamFlag))
		}

		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DPLATFORM=%s", platform))
	}

	if systemNameFlag == "android" {
		if _, err := os.Stat(androidNdkDir); errors.Is(err, os.ErrNotExist) {
			glog.Fatalf("Android Ndk Directory at %s demanded", androidNdkDir);
		}
		glog.Infof("Using android ndk dir %s", androidNdkDir);
		androidAbiTarget, androidAppAbi = getAndroidTargetAndAppAbi(archFlag)
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_TOOLCHAIN_FILE=%s/../cmake/platforms/Android.cmake", buildDir))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DANDROID_ABI=%s", androidAppAbi))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DANDROID_ABI_TARGET=%s", androidAbiTarget))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DANDROID_API_VERSION=%d", androidApiLevel))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DANDROID_NDK_ROOT=%s", androidNdkDir))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DANDROID_CURRENT_OS=%s", runtime.GOOS))

		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DLLVM_SYSROOT=%s", clangPath))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_SYSTEM_PROCESSOR=%s", androidAppAbi))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_TARGET=%s%d", androidAbiTarget, androidApiLevel))
		// FIXME patch llvm toolchain to find libunwind.a
		getAndFixAndroidLibunwind(androidNdkDir);
	}

	if systemNameFlag == "harmony" {
		if harmonyNdkDir == "" {
			glog.Fatalf("Harmony Ndk Directory demanded");
		}
		harmonyAbiTarget, harmonyAppAbi = getHarmonyTargetAndAppAbi(archFlag)
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_TOOLCHAIN_FILE=%s/../cmake/platforms/Harmony.cmake", buildDir))
		// hard-coded
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DOHOS_APILEVEL=%s", "9"))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DOHOS_ARCH=%s", harmonyAppAbi))

		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DOHOS_SDK_NATIVE=%s/native", harmonyNdkDir))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DLLVM_SYSROOT=%s", clangPath))
		getAndFixHarmonyLibunwind()
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
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DLLVM_SYSROOT=%s", clangPath))
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
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DFREEBSD_ABI_VERSION=%d", freebsdAbiFlag))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DLLVM_SYSROOT=%s", clangPath))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_SYSROOT=%s", sysrootFlag))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_SYSTEM_PROCESSOR=%s", llvmArch))
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DGCC_TARGET=%s", llvmTarget))
	}
	cmakeCmd := append([]string{"cmake", ".."}, cmakeArgs...)
	if noConfigureFlag {
		return
	}
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
	cmakeCmd := []string{"cmake", "--build", ".",
		"--config", cmakeBuildTypeFlag,
		"--parallel", fmt.Sprintf("%d", cmakeBuildConcurrencyFlag),
		"--target", APPNAME}
	if !(systemNameFlag == "ios" && (runBenchmarkFlag || runTestFlag)) {
		cmdRun(cmakeCmd, true)
	}
	if buildBenchmarkFlag || runBenchmarkFlag {
		cmakeCmd := []string{"cmake", "--build", ".",
			"--config", cmakeBuildTypeFlag,
			"--parallel", fmt.Sprintf("%d", cmakeBuildConcurrencyFlag),
			"--target", "yass_benchmark"}
		if !(systemNameFlag == "ios" && runBenchmarkFlag) {
			cmdRun(cmakeCmd, true)
		}
	}
	if runBenchmarkFlag {
		if systemNameFlag == "ios" && subSystemNameFlag == "simulator" {
			xcodeCmd := []string{"xcodebuild", "test", "-configuration", cmakeBuildTypeFlag,
				"-jobs", fmt.Sprintf("%d", cmakeBuildConcurrencyFlag),
				"-scheme", "yass", "-destination", "platform=iOS Simulator,name=iPhone 15 Pro"}
			if (!runTestFlag) {
				cmdRun(xcodeCmd, true)
			}
		} else if systemNameFlag == "ios" {
			xcodeCmd := []string{"xcodebuild", "test", "-configuration", cmakeBuildTypeFlag,
				"-jobs", fmt.Sprintf("%d", cmakeBuildConcurrencyFlag),
				"-scheme", "yass", "-destination", "platform=iOS,name=" + iosTestDeviceNameFlag}
			if (!runTestFlag) {
				cmdRun(xcodeCmd, true)
			}
		} else {
			checkCmd := []string{"./yass_benchmark"}
			if verboseFlag > 0 {
				checkCmd = []string{"./yass_benchmark", "-v", fmt.Sprintf("%d", verboseFlag), "-logtostderr"}
			}
			cmdRun(checkCmd, true)
		}
	}
	if buildTestFlag || runTestFlag {
		cmakeCmd := []string{"cmake", "--build", ".",
			"--config", cmakeBuildTypeFlag,
			"--parallel", fmt.Sprintf("%d", cmakeBuildConcurrencyFlag),
			"--target", "yass_test"}
		if !(systemNameFlag == "ios" && runTestFlag) {
			cmdRun(cmakeCmd, true)
		}
	}
	if runTestFlag {
		if systemNameFlag == "ios" && subSystemNameFlag == "simulator" {
			xcodeCmd := []string{"xcodebuild", "test", "-configuration", cmakeBuildTypeFlag,
				"-jobs", fmt.Sprintf("%d", cmakeBuildConcurrencyFlag),
				"-scheme", "yass", "-destination", "platform=iOS Simulator,name=iPhone 15 Pro"}
			cmdRun(xcodeCmd, true)
		} else if systemNameFlag == "ios" {
			xcodeCmd := []string{"xcodebuild", "test", "-configuration", cmakeBuildTypeFlag,
				"-jobs", fmt.Sprintf("%d", cmakeBuildConcurrencyFlag),
				"-scheme", "yass", "-destination", "platform=iOS,name=" + iosTestDeviceNameFlag}
			cmdRun(xcodeCmd, true)
		} else {
			checkCmd := []string{"./yass_test"}
			if verboseFlag > 0 {
				checkCmd = []string{"./yass_test", "-v", fmt.Sprintf("%d", verboseFlag), "-logtostderr"}
			}
			cmdRun(checkCmd, true)
		}
	}
	// FIXME move to cmake (required by Xcode generator)
	if systemNameFlag == "darwin" {
		if _, err := os.Stat(filepath.Join(cmakeBuildTypeFlag, getAppName())); err == nil {
			err := renameByUnlink(filepath.Join(cmakeBuildTypeFlag, getAppName()), getAppName())
			glog.Fatalf("%v", err)
		}
	}
}

func GetWin32SearchPath() []string {
	// no search when using libc++,
	// TODO test under shaded libc++

	// VCToolsVersion:PlatformToolchainversion:VisualStudioVersion
	//  14.30-14.3?:v143:Visual Studio 2022
	//  14.20-14.29:v142:Visual Studio 2019
	//  14.10-14.19:v141:Visual Studio 2017
	//  14.00-14.00:v140:Visual Studio 2015
	//
	//  From wiki: https://en.wikipedia.org/wiki/Microsoft_Visual_C%2B%2B
	//  https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=msvc-170#microsoft-specific-predefined-macros
	//  6.00    Visual Studio 6.0                          1200
	//  7.00    Visual Studio .NET 2002 (7.0)              1300
	//  7.10    Visual Studio .NET 2003 (7.1)              1310
	//  8.00    Visual Studio 2005 (8.0)                   1400
	//  9.00    Visual Studio 2008 (9.0)                   1500
	//  10.00   Visual Studio 2010 (10.0)                  1600
	//  12.00   Visual Studio 2013 (12.0)                  1800
	//  14.00   Visual Studio 2015 (14.0)                  1900
	//  14.10   Visual Studio 2017 RTW (15.0)              1910
	//  14.11   Visual Studio 2017 version 15.3            1911
	//  14.12   Visual Studio 2017 version 15.5            1912
	//  14.13   Visual Studio 2017 version 15.6            1913
	//  14.14   Visual Studio 2017 version 15.7            1914
	//  14.15   Visual Studio 2017 version 15.8            1915
	//  14.16   Visual Studio 2017 version 15.9            1916
	//  14.20   Visual Studio 2019 RTW (16.0)              1920
	//  14.21   Visual Studio 2019 version 16.1            1921
	//  14.22   Visual Studio 2019 version 16.2            1922
	//  14.23   Visual Studio 2019 version 16.3            1923
	//  14.24   Visual Studio 2019 version 16.4            1924
	//  14.25   Visual Studio 2019 version 16.5            1925
	//  14.26   Visual Studio 2019 version 16.6            1926
	//  14.27   Visual Studio 2019 version 16.7            1927
	//  14.28   Visual Studio 2019 version 16.8, 16.9      1928
	//  14.29   Visual Studio 2019 version 16.10, 16.11    1929
	//  14.30   Visual Studio 2022 RTW (17.0)              1930
	//  14.31   Visual Studio 2022 version 17.1            1931
	//
	//  Visual Studio 2015 is not supported by this script due to
	//  the missing environment variable VCToolsVersion
	vctoolsVersionStr := os.Getenv("VCToolsVersion")
	var vctoolsVersion float64
	if len(vctoolsVersionStr) >= 4 {
		// for vc141 or above, VCToolsVersion=14.??.xxxx
		vctoolsVersion, _ = strconv.ParseFloat(vctoolsVersionStr[:5], 32)
	} else {
		// for vc140 or below, VCToolsVersion=14.0
		vctoolsVersion, _ = strconv.ParseFloat(vctoolsVersionStr, 32)
	}

	var platformToolchainVersion string
	if vctoolsVersion >= 14.30 {
		platformToolchainVersion = "143"
	} else if vctoolsVersion >= 14.20 && vctoolsVersion < 14.30 {
		platformToolchainVersion = "142"
	} else if vctoolsVersion >= 14.10 && vctoolsVersion < 14.20 {
		platformToolchainVersion = "141"
	} else if vctoolsVersion >= 14.00 && vctoolsVersion < 14.10 {
		platformToolchainVersion = "140"
	} else {
		glog.Fatalf("unsupported vctoolchain version %f", vctoolsVersion)
	}

	// Environment variable VCToolsRedistDir isn't correct when it doesn't match
	// Visual Studio's default toolchain.
	// according to microsoft's design issue, don't use it.
	//
	// for example:
	//
	// VCINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\
	// VCToolsInstallDir=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.16.27023\
	// VCToolsRedistDir=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\14.30.30704\
	//
	// for vc140 it's:
	// C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\redist
	var vcredistDir string
	if vctoolsVersion >= 14.30 {
		vcredistDir = os.Getenv("VCToolsRedistDir")
	} else if vctoolsVersion >= 14.20 && vctoolsVersion < 14.30 {
		vcredistDir = filepath.Join(os.Getenv("VCINSTALLDIR"), "Redist", "MSVC", os.Getenv("VCToolsVersion"))
		// fallback
		if _, err := os.Stat(vcredistDir); errors.Is(err, os.ErrNotExist) {
			vcredistDir = filepath.Join(os.Getenv("VCINSTALLDIR"), "Redist", "MSVC", "14.29.30133")
		}
	} else if vctoolsVersion >= 14.10 && vctoolsVersion < 14.20 {
		vcredistDir = filepath.Join(os.Getenv("VCINSTALLDIR"), "Redist", "MSVC", os.Getenv("VCToolsVersion"))
		// fallback
		if _, err := os.Stat(vcredistDir); errors.Is(err, os.ErrNotExist) {
			vcredistDir = filepath.Join(os.Getenv("VCINSTALLDIR"), "Redist", "MSVC", "14.16.27012")
		}
	} else if vctoolsVersion >= 14.00 && vctoolsVersion < 14.10 {
		vcredistDir = filepath.Join(os.Getenv("VCINSTALLDIR"), "redist")
	} else {
		vcredistDir = ""
	}

	// Search Path (VC Runtime and MFC) (vctoolsVersion newer than v140)
	// C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\14.30.30704\debug_nonredist\x86\Microsoft.VC143.DebugMFC
	// C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\14.30.30704\debug_nonredist\x86\Microsoft.VC143.DebugCRT
	// C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\14.30.30704\x86\Microsoft.VC143.MFCLOC
	// C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\14.30.30704\x86\Microsoft.VC143.MFC
	// C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\14.30.30704\x86\Microsoft.VC143.CRT
	searchDirs := []string{
		filepath.Join(vcredistDir, "debug_nonredist", msvcTargetArchFlag, fmt.Sprintf("Microsoft.VC%s.DebugMFC", platformToolchainVersion)),
		filepath.Join(vcredistDir, "debug_nonredist", msvcTargetArchFlag, fmt.Sprintf("Microsoft.VC%s.DebugCRT", platformToolchainVersion)),
		filepath.Join(vcredistDir, msvcTargetArchFlag, fmt.Sprintf("Microsoft.VC%s.MFCLOC", platformToolchainVersion)),
		filepath.Join(vcredistDir, msvcTargetArchFlag, fmt.Sprintf("Microsoft.VC%s.MFC", platformToolchainVersion)),
		filepath.Join(vcredistDir, msvcTargetArchFlag, fmt.Sprintf("Microsoft.VC%s.CRT", platformToolchainVersion)),
	}

	// remove the trailing slash
	sdkVersionInt := os.Getenv("WindowsSDKVersion")
	sdkVersion := sdkVersionInt[:len(sdkVersionInt)-1]
	sdkBaseDir := os.Getenv("WindowsSdkDir")

	// Search Path (UCRT)
	//
	// Please note The UCRT files are not redistributable for ARM64 Win32.
	// https://chromium.googlesource.com/chromium/src/+/lkgr/build/win/BUILD.gn
	// C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x86\ucrt
	// C:\Program Files (x86)\Windows Kits\10\Redist\10.0.19041.0\ucrt\DLLS\x86
	// C:\Program Files (x86)\Windows Kits\10\ExtensionSDKs\Microsoft.UniversalCRT.Debug\10.0.19041.0\Redist\Debug\x86
	searchDirs = append(searchDirs, filepath.Join(sdkBaseDir, "bin", sdkVersion, msvcTargetArchFlag, "ucrt"))
	searchDirs = append(searchDirs, filepath.Join(sdkBaseDir, "Redist", sdkVersion, "ucrt", "DLLS", msvcTargetArchFlag))
	searchDirs = append(searchDirs, filepath.Join(sdkBaseDir, "ExtensionSDKs", "Microsoft.UniversalCRT.Debug", sdkVersion, "Redist", "Debug", msvcTargetArchFlag))

	// Fallback search path for XP (v140_xp, v141_xp)
	// Refer to #27, https://github.com/Chilledheart/yass/issues/27
	// $project_root\third_party\vcredist\x86
	if vctoolsVersion >= 14.00 && vctoolsVersion < 14.20 {
		path, _ := filepath.Abs(filepath.Join("..", "third_party", "vcredist", msvcTargetArchFlag))
		searchDirs = append(searchDirs, path)
	}
	return searchDirs
}

// The output of dumpbin /dependents is like below:
// Microsoft (R) COFF/PE Dumper Version 14.30.30709.0
// Copyright (C) Microsoft Corporation.  All rights reserved.
// Dump of file arm64-windows-yass.exe
// File Type: EXECUTABLE IMAGE
//   Image has the following dependencies:
//     WS2_32.dll
//     GDI32.dll
//     SHELL32.dll
//     KERNEL32.dll
//     USER32.dll
//     ADVAPI32.dll
//     MSVCP140.dll
//     MSWSOCK.dll
//     mfc140u.dll
//     dbghelp.dll
//     VCRUNTIME140.dll
//     api-ms-win-crt-runtime-l1-1-0.dll
//     api-ms-win-crt-heap-l1-1-0.dll
//     api-ms-win-crt-stdio-l1-1-0.dll
//     api-ms-win-crt-math-l1-1-0.dll
//     api-ms-win-crt-time-l1-1-0.dll
//     api-ms-win-crt-filesystem-l1-1-0.dll
//     api-ms-win-crt-convert-l1-1-0.dll
//     api-ms-win-crt-environment-l1-1-0.dll
//     api-ms-win-crt-string-l1-1-0.dll
//     api-ms-win-crt-locale-l1-1-0.dll
//   Summary
//        15000 .data
//         4000 .pdata
//        2C000 .rdata
//         2000 .reloc
//        12000 .rsrc
//        71000 .text
//
func GetDependenciesByDumpbin(path string, searchDirs []string) ([]string, []string) {
	lines := strings.Split(cmdCheckOutput([]string{"dumpbin", "/nologo", "/dependents", path}), "\n")
	dlls := []string{}
	resolvedDlls := []string{}
	unresolvedDlls := []string{}
	systemDlls := []string {"WS2_32.dll", "GDI32.dll", "SHELL32.dll", "USER32.dll",
		"ADVAPI32.dll", "MSWSOCK.dll", "dbghelp.dll", "KERNEL32.dll",
		"ole32.dll", "OLEAUT32.dll", "SHLWAPI.dll", "IMM32.dll",
		"UxTheme.dll", "PROPSYS.dll", "dwmapi.dll", "WININET.dll",
		"OLEACC.dll", "ODBC32.dll", "oledlg.dll", "urlmon.dll",
		"MSIMG32.dll", "WINMM.dll", "CRYPT32.dll", "gdiplus.dll",
		"COMCTL32.dll", "RASAPI32.dll", "IPHLPAPI.DLL",
		"WINHTTP.dll", "VERSION.dll", "POWRPROF.dll",
	}
	systemDllsMap := make(map[string]struct{})
	for _, dll := range(systemDlls) {
		systemDllsMap[strings.ToLower(dll)] = struct{}{}
	}

	p := regexp.MustCompile("(?i)    (\\S+.dll)")
	for _, line := range(lines) {
		matches := p.FindStringSubmatch(line)
		if len(matches) > 1 {
			_, ok := systemDllsMap[strings.ToLower(matches[1])]
			if !ok {
				dlls = append(dlls, matches[1])
			}
		}
	}

	// append crt dlls
	if msvcCrtLinkageFlag == "dynamic" {
		vctoolsVersionStr := os.Getenv("VCToolsVersion")
		var vctoolsVersion float64
		if len(vctoolsVersionStr) >= 4 {
			// for vc141 or above, VCToolsVersion=14.??.xxxx
			vctoolsVersion, _ = strconv.ParseFloat(vctoolsVersionStr[:5], 32)
		} else {
			// for vc140 or below, VCToolsVersion=14.0
			vctoolsVersion, _ = strconv.ParseFloat(vctoolsVersionStr, 32)
		}

		if vctoolsVersion >= 14.30 {
			if msvcTargetArchFlag == "x64" || msvcTargetArchFlag == "arm64" {
				dlls = append(dlls, "concrt140.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
							"msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll", "vccorlib140.dll", "vcruntime140.dll", "vcruntime140_1.dll")
			} else if msvcTargetArchFlag == "x86" {
				dlls = append(dlls, "concrt140.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
							"msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll", "vccorlib140.dll", "vcruntime140.dll")
			}
		} else if vctoolsVersion >= 14.20 && vctoolsVersion < 14.30 {
			if msvcTargetArchFlag == "x64" || msvcTargetArchFlag == "arm64" {
				dlls = append(dlls, "concrt140.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
							"msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll", "vccorlib140.dll", "vcruntime140.dll", "vcruntime140_1.dll")
			} else if msvcTargetArchFlag == "x86" {
				dlls = append(dlls, "concrt140.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
							"msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll", "vccorlib140.dll", "vcruntime140.dll")
			}
		} else if vctoolsVersion >= 14.10 && vctoolsVersion < 14.20 {
			if msvcTargetArchFlag == "x64" {
				dlls = append(dlls, "concrt140.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
							"vccorlib140.dll", "vcruntime140.dll")
			} else if msvcTargetArchFlag == "x86" {
				dlls = append(dlls, "concrt140.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
							"vccorlib140.dll", "vcruntime140.dll")
			}
		} else if vctoolsVersion >= 14.00 && vctoolsVersion < 14.10 {
			if msvcTargetArchFlag == "x64" {
				dlls = append(dlls, "concrt140.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
							"vccorlib140.dll", "vcruntime140.dll")
			} else if msvcTargetArchFlag == "x86" {
				dlls = append(dlls, "concrt140.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
							"vccorlib140.dll", "vcruntime140.dll")
			}
		} else {
			// nop
		}

		if cmakeBuildTypeFlag == "Debug" {
			dlls = append(dlls, "ucrtbased.dll")
		} else {
			dlls = append(dlls, "ucrtbase.dll")
			dlls = append(dlls, "api-ms-win-crt-conio-l1-1-0.dll",
					"api-ms-win-crt-convert-l1-1-0.dll",
					"api-ms-win-crt-environment-l1-1-0.dll",
					"api-ms-win-crt-filesystem-l1-1-0.dll",
					"api-ms-win-crt-heap-l1-1-0.dll",
					"api-ms-win-crt-locale-l1-1-0.dll",
					"api-ms-win-crt-math-l1-1-0.dll",
					"api-ms-win-crt-multibyte-l1-1-0.dll",
					"api-ms-win-crt-private-l1-1-0.dll",
					"api-ms-win-crt-process-l1-1-0.dll",
					"api-ms-win-crt-runtime-l1-1-0.dll",
					"api-ms-win-crt-stdio-l1-1-0.dll",
					"api-ms-win-crt-string-l1-1-0.dll",
					"api-ms-win-crt-time-l1-1-0.dll",
					"api-ms-win-crt-utility-l1-1-0.dll")
		}

	}

	for _, dll := range(dlls) {
		resolved := false
		for _, searchDir := range(searchDirs) {
			d := filepath.Join(searchDir, dll)
			if _, err := os.Stat(d); !errors.Is(err, os.ErrNotExist) {
				resolvedDlls = append(resolvedDlls, d)
				resolved = true;
				break
			}
		}
		if !resolved {
			unresolvedDlls = append(unresolvedDlls, dll)
		}
	}
	return resolvedDlls, unresolvedDlls
}

func postStateCopyDependedLibraries() {
	glog.Info("PostState -- Copy Depended Libraries")
	glog.Info("======================================================================")
	searchDirs := []string{}
	if systemNameFlag == "windows" {
		searchDirs = append(searchDirs, GetWin32SearchPath()...)
		for _, searchDir := range(searchDirs) {
			glog.Infof("--- %s", searchDir)
		}
		deps, unresolvedDeps := GetDependenciesByDumpbin(getAppName(), searchDirs)

		hasCrashpad := true
		if _, err := os.Stat("crashpad_handler.exe"); errors.Is(err, os.ErrNotExist) {
			hasCrashpad = false
		}
		if hasCrashpad {
			deps = append(deps, "crashpad_handler.exe")
		}

		depsMap := make(map[string]struct{})
		for _, dep := range(deps) {
			depsMap[dep] = struct{}{}
		}
		for true {
			depsExtended := deps
			for _, dep := range(deps) {
				depDeps, depUnresolvedDeps := GetDependenciesByDumpbin(dep, searchDirs)
				depsExtended = append(depsExtended, depDeps...)
				unresolvedDeps = append(unresolvedDeps, depUnresolvedDeps...)
			}
			depsExtendedMap := make(map[string]struct{})
			for _, dep := range(depsExtended) {
				depsExtendedMap[dep] = struct{}{}
			}
			if reflect.DeepEqual(depsMap, depsExtendedMap) {
				break;
			}
			depsExtended = []string{}
			for dep := range(depsExtendedMap) {
				depsExtended = append(depsExtended, dep)
			}
			deps = depsExtended
			depsMap = depsExtendedMap
		}
		unresolvedDepsMap := make(map[string]struct{})
		for _, unresolvedDep := range(unresolvedDeps) {
			unresolvedDepsMap[unresolvedDep] = struct{}{}
		}
		glog.Infof("--- Dependent dll")
		for _, dep := range(deps) {
			glog.Infof("--- --- %s", dep)
			if !strings.HasSuffix(strings.ToLower(dep), ".dll") {
				continue;
			}
			err := copyFile(dep, filepath.Base(dep))
			if err != nil {
				glog.Fatalf("Copy file %s failed :%v", dep, err)
			}
		}
		glog.Infof("--- Missing dependent dll")
		for unresolvedDep, _ := range(unresolvedDepsMap) {
			glog.Infof("--- --- %s", unresolvedDep)
		}
	// TBD
	}
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
	if systemNameFlag == "ios" {
		glog.Info("Done in xcodebuild")
		return
	}
	if systemNameFlag == "mingw" || systemNameFlag == "android" || systemNameFlag == "harmony" || systemNameFlag == "linux" || systemNameFlag == "freebsd" {
		objcopy := filepath.Join(clangPath, "bin", "llvm-objcopy")
		if runtime.GOOS == "windows" {
			objcopy = filepath.Join(clangPath, "bin", "llvm-objcopy.exe")
		}
		if _, err := os.Stat(objcopy); errors.Is(err, os.ErrNotExist) {
			objcopy = "objcopy"
		}
		// create a file containing the debugging info.
		cmdRun([]string{objcopy, "--only-keep-debug", getAppName(), getAppName() + ".dbg"}, false)
		// stripped executable.
		cmdRun([]string{objcopy, "--strip-debug", getAppName()}, false)
		// to add a link to the debugging info into the stripped executable.
		cmdRun([]string{objcopy, "--add-gnu-debuglink=" + getAppName() + ".dbg", getAppName()}, false)

		// strip crashpad_handler as well if any
		hasCrashpad := true
		crashpadPath := "crashpad_handler"
		if _, err := os.Stat(crashpadPath); errors.Is(err, os.ErrNotExist) {
			hasCrashpad = false
		}
		if hasCrashpad {
			cmdRun([]string{objcopy, "--strip-debug", crashpadPath}, false)
		}
	} else if systemNameFlag == "darwin" {
		cmdRun([]string{"dsymutil", filepath.Join(getAppName(), "Contents", "MacOS", APPNAME),
			"--statistics", "--papertrail", "-o", getAppName() + ".dSYM"}, false)
		cmdRun([]string{"strip", "-S", "-x", "-v", filepath.Join(getAppName(), "Contents", "MacOS", APPNAME)}, false)

		// strip crashpad_handler as well if any
		hasCrashpad := true
		crashpadPath := filepath.Join(getAppName(), "Contents", "Resources", "crashpad_handler")
		if _, err := os.Stat(crashpadPath); errors.Is(err, os.ErrNotExist) {
			hasCrashpad = false
		}
		if hasCrashpad {
			cmdRun([]string{"strip", "-S", "-x", "-v", crashpadPath}, false)
		}
	} else if systemNameFlag == "ios" {
		cmdRun([]string{"dsymutil", filepath.Join(getAppName(), APPNAME),
			"--statistics", "--papertrail", "-o", getAppName() + ".dSYM"}, false)
		cmdRun([]string{"strip", "-S", "-x", "-v", filepath.Join(getAppName(), APPNAME)}, false)
	} else {
		glog.Warningf("not supported in platform %s", systemNameFlag)
	}
}

func postStateCodeSign() {
	glog.Info("PostState -- Code Sign")
	glog.Info("======================================================================")
	if systemNameFlag == "ios" {
		glog.Info("Done in xcodebuild")
		return
	}
	if cmakeBuildTypeFlag != "Release" || (systemNameFlag != "darwin" && systemNameFlag != "ios") {
		return
	}
	// reference https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution/resolving_common_notarization_issues?language=objc
	// Hardened runtime is available in the Capabilities pane of Xcode 10 or later
	// code sign crashpad_handler as well if any
	hasCrashpad := true
	crashpadPath := filepath.Join(getAppName(), "Contents", "Resources", "crashpad_handler")
	if _, err := os.Stat(crashpadPath); errors.Is(err, os.ErrNotExist) {
		hasCrashpad = false
	}
	codesignCmd := []string{
		"codesign", "-s", macosxCodeSignIdentityFlag,
		"--deep", "--force", "--options=runtime", "--timestamp",
		"--entitlements=" + filepath.Join(projectDir, "src", "mac", "entitlements.plist"),
	}
	if (macosxKeychainPathFlag != "") {
		codesignCmd = append(codesignCmd, "--keychain", macosxKeychainPathFlag)
	}

	if hasCrashpad {
		codesignCmd := append(codesignCmd, crashpadPath)
		cmdRun(codesignCmd, true)
	}

	codesignCmd = append(codesignCmd, getAppName())
	cmdRun(codesignCmd, true)
	cmdRun([]string{"codesign", "-dv", "--deep", "--strict", "--verbose=4", getAppName()}, true)
	cmdRun([]string{"codesign", "-d", "--entitlements", ":-", getAppName()}, true)

	if hasCrashpad {
		cmdRun([]string{"codesign", "-d", "--entitlements", ":-", crashpadPath}, true)
	}
	cmdRun([]string{"spctl", "-a", "-vvv", "--type", "install", getAppName()}, false)
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

// LICENSEs use unified LICENSE
func postStateArchiveLicenses() []string {
	var licenses []string
	licenses = append(licenses, "LICENSE")
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
		cmd := []string{"mkdir", "-p", prefix}
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

func archiveMainFile(output string, prefix string, paths []string, dllPaths []string) {
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
		cmdRun([]string{"../scripts/pkg-dmg",
			"--source", paths[0],
			"--target", output,
			"--sourcefile",
			"--volname", "Yet Another Shadow Socket",
			"--resource", "eula.xml",
			"--icon", "../src/mac/yass.icns",
			"--copy", "../macos/.DS_Store:/.DS_Store",
			"--copy", "../macos/.background:/",
			"--symlink", "/Applications:/Applications"}, true)
	} else if systemNameFlag == "ios" {
		cmdRun([]string{"xcodebuild", "archive",
			"-archivePath", cmakeBuildTypeFlag + ".xcarchive",
			"-configuration", cmakeBuildTypeFlag,
			"-jobs", fmt.Sprintf("%d", cmakeBuildConcurrencyFlag),
			"-target", APPNAME, "-scheme", APPNAME}, true)

		cmdRun([]string{"xcodebuild", "-exportArchive",
			"-archivePath", cmakeBuildTypeFlag + ".xcarchive",
			"-exportPath", ".",
			"-exportOptionsPlist", "../tools/development.plist"}, true)
		err := os.Rename("Yet Another Shadow Socket.ipa", output)
		if err != nil {
			glog.Fatalf("%v", err)
		}
		// cleanup after exportArchive
		//
		buildSubdir := cmakeBuildTypeFlag + "-iphoneos"
		if subSystemNameFlag == "simulator" {
			buildSubdir = cmakeBuildTypeFlag + "-iphonesimulator"
		}
		err = os.Chdir(buildSubdir)
		if err != nil {
			glog.Fatalf("%v", err)
		}
		cmdRun([]string{"rm", "-f", "YassPacketTunnel.appex", "libasio.a",
			"libyass_crashpad.a", "libyass_core.a", "libyass_net.a", "yass.app"}, true)
		err = os.Chdir(buildDir)
		if err != nil {
			glog.Fatalf("%v", err)
		}
	} else if systemNameFlag == "android" && variantFlag == "gui" {
		androidDir := "../android"
		err := os.Chdir(androidDir)
		if err != nil {
			glog.Fatalf("%v", err)
		}
		if (androidSdkDir != "") {
			glog.Infof("android sdk dir to %s", androidSdkDir)
			localProperties := fmt.Sprintf("sdk.dir=%s\n", androidSdkDir)
			if (androidNdkDir != "") {
				localProperties += fmt.Sprintf("ndk.dir=%s\n", androidNdkDir)
			}
			err = ioutil.WriteFile("local.properties", []byte(localProperties), 0666)
			if err != nil {
				glog.Fatalf("%v", err)
			}
		}
		_, abi := getAndroidTargetAndAppAbi(archFlag)
		if cmakeBuildTypeFlag == "Release" || cmakeBuildTypeFlag == "MinSizeRel" {
			cmdRun([]string{"./gradlew", "yass:assembleRelease", "--info"}, true)
			err = os.Rename(fmt.Sprintf("./yass/build/outputs/apk/release/yass-%s-release-unsigned.apk", abi), output)
			if err != nil {
				glog.Fatalf("%v", err)
			}
		} else {
			cmdRun([]string{"./gradlew", "yass:assembleDebug", "--info"}, true)
			err = os.Rename(fmt.Sprintf("./yass/build/outputs/apk/debug/yass-%s-release-unsigned.apk", abi), output)
			if err != nil {
				glog.Fatalf("%v", err)
			}
		}
		err = os.Chdir(buildDir)
		if err != nil {
			glog.Fatalf("%v", err)
		}
	} else {
		paths = append(paths, dllPaths...)
		archiveFiles(output, prefix, paths)
	}
}

func generateMsi(output string, dllPaths []string, licensePaths []string, hasCrashpad bool, hasIcuDataFile bool) {
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
	if (hasCrashpad) {
		wxsXml = strings.Replace(wxsXml, "<!-- %CRASHPAD_HANDLER_HOLDER% -->", "<File Name='crashpad_handler.exe' Source='crashpad_handler.exe' KeyPath='no'/>", 1)
	}
	if (hasIcuDataFile) {
		wxsXml = strings.Replace(wxsXml, "<!-- %ICUDATAFILE_HOLDER% -->", "<File Name='icudtl.dat' Source='icudtl.dat' KeyPath='no'/>", 1)
	}

	err = ioutil.WriteFile("yass.wxs", []byte(wxsXml), 0666)
	if err != nil {
		glog.Fatalf("%v", err)
	}

	glog.Info("Feeding WiX compiler...")
	cmdRun([]string{"candle.exe", "yass.wxs", "-dPlatform=" + msvcTargetArchFlag, "-dVersion=" + tagFlag}, true)

	glog.Info("Generating MSI file...")
	cmdRun([]string{"light.exe", "-ext", "WixUIExtension", "-out", output, "-cultures:en-US", "-sice:ICE03", "-sice:ICE57", "-sice:ICE61", "yass.wixobj"}, true)
}

func generateNSIS(output string, dllPaths []string) {
	nsiTemplate, err := ioutil.ReadFile(filepath.Join("..", "yass.nsi"))
	if err != nil {
		glog.Fatalf("%v", err)
	}
	nsiContent := string(nsiTemplate)
	nsiContent = strings.Replace(nsiContent, "yass-installer.exe", output, 1)

	dllReplacement := ""
	for _, dllPath := range dllPaths {
		dllReplacement += fmt.Sprintf("File \"%s\"\n", dllPath)
	}

	nsiContent = strings.Replace(nsiContent, "# DLL PLACEHOLDER", dllReplacement, 1)

	err = ioutil.WriteFile("yass.nsi", []byte(nsiContent), 0666)
	if err != nil {
		glog.Fatalf("%v", err)
	}
	glog.Info("Feeding NSIS compiler...")
	cmdRun([]string{"C:\\Program Files (x86)\\NSIS\\makensis.exe", "/XSetCompressor /FINAL lzma", "yass.nsi"}, true)
}

func generateNSISSystemInstaller(output string) {
	glog.Info("Feeding CPack NSIS compiler...")
	cmdRun([]string{"C:\\Program Files\\CMake\\bin\\cpack.exe"}, true)

	if msvcTargetArchFlag == "x86" {
		os.Rename(fmt.Sprintf("yass-%s-win32.exe", tagFlag), output)
	} else if msvcTargetArchFlag == "x64" {
		os.Rename(fmt.Sprintf("yass-%s-win64.exe", tagFlag), output)
	} else {
		glog.Fatalf("Unsupported msvc arch: %s for nsis builder", msvcTargetArchFlag)
	}
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
	} else if systemNameFlag == "mingw" {
		osName := "mingw"
		if mingwAllowXpFlag {
			osName = "mingw-winxp"
		}
		archiveFormat = fmt.Sprintf("%%s-%s-release-%s-%s%%s%%s", osName, archFlag, tag)
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

	if systemNameFlag == "android" || systemNameFlag == "harmony" || systemNameFlag == "linux" || systemNameFlag == "freebsd" {
		ext = ".tgz"
	}

	archive := fmt.Sprintf(archiveFormat, APPNAME, "", ext)
	archivePrefix := fmt.Sprintf(archiveFormat, strings.Replace(APPNAME, "_", "-", 1), "", "")
	archiveSuffix := fmt.Sprintf(archiveFormat, "", "", "")
	archiveSuffix = archiveSuffix[1:]
	if systemNameFlag == "android" && variantFlag == "gui" {
		archive = fmt.Sprintf(archiveFormat, APPNAME, "", ".apk")
	}
	if systemNameFlag == "darwin" {
		archive = fmt.Sprintf(archiveFormat, APPNAME, "", ".dmg")
	}
	if systemNameFlag == "ios" {
		archive = fmt.Sprintf(archiveFormat, APPNAME, "", ".ipa")
	}
	hasCrashpad := true
	if _, err := os.Stat("crashpad_handler.exe"); errors.Is(err, os.ErrNotExist) {
		hasCrashpad = false
	}
	hasIcuDataFile := true
	if _, err := os.Stat("icudtl.dat"); errors.Is(err, os.ErrNotExist) {
		hasIcuDataFile = false
	}

	msiArchive := fmt.Sprintf(archiveFormat, APPNAME, "", ".msi")
	nsisArchive := fmt.Sprintf(archiveFormat, APPNAME, "-user-installer", ".exe")
	debugArchive := fmt.Sprintf(archiveFormat, APPNAME, "-debuginfo", ext)
	nsisSystemArchive := fmt.Sprintf(archiveFormat, APPNAME, "-system-installer", ".exe")

	archive = filepath.Join("..", archive)
	msiArchive = filepath.Join("..", msiArchive)
	nsisArchive = filepath.Join("..", nsisArchive)
	debugArchive = filepath.Join("..", debugArchive)
	nsisSystemArchive = filepath.Join("..", nsisSystemArchive)

	archives := map[string][]string{}

	paths := []string{getAppName()}
	var dllPaths []string
	var dbgPaths []string

	if systemNameFlag == "windows" {
		entries, _ := os.ReadDir("./")
		for _, entry := range(entries) {
			name := entry.Name()
			iname := strings.ToLower(name)
			if strings.HasSuffix(iname, ".dll") {
				dllPaths = append(dllPaths, name)
			}
		}
	}

	// copying manpages if any
	if (variantFlag == "cli" || variantFlag == "server") && (systemNameFlag == "linux" || systemNameFlag == "freebsd") {
		paths = append(paths, fmt.Sprintf("../doc/%s.1", APPNAME))
	}
	// copying dependent crashpad handler if any
	if hasCrashpad {
		paths = append(paths, "crashpad_handler.exe")
	}
	// copying dependent icudata
	if hasIcuDataFile {
		paths = append(paths, "icudtl.dat")
	}

	// copying dependent LICENSEs
	licensePaths := postStateArchiveLicenses()
	paths = append(paths, licensePaths...)

	// main bundle
	archiveMainFile(archive, archivePrefix, paths, dllPaths)
	archives[archive] = paths

	// msi installer
	// FIXME wixtoolset3.14 supports arm64 but only 3.11 is out for release
	// https://github.com/wixtoolset/issues/issues/5558
	// error CNDL0265 : The Platform attribute has an invalid value arm64.
	// Possible values are x86, x64, or ia64.
	if systemNameFlag == "windows" && msvcTargetArchFlag != "arm64" {
		generateMsi(msiArchive, dllPaths, licensePaths, hasCrashpad, hasIcuDataFile)
		archives[msiArchive] = []string{msiArchive}
	}
	// nsis installer
	if systemNameFlag == "windows" && msvcTargetArchFlag != "arm64" {
		generateNSIS(nsisArchive, dllPaths)
		archives[nsisArchive] = []string{nsisArchive}
		generateNSISSystemInstaller(nsisSystemArchive)
		archives[nsisSystemArchive] = []string{nsisSystemArchive}
	}
	// debuginfo file
	if systemNameFlag == "windows" {
		archiveFiles(debugArchive, archivePrefix, []string{APPNAME + ".pdb"})
		dbgPaths = append(dbgPaths, APPNAME+".pdb")
	} else if systemNameFlag == "mingw" || systemNameFlag == "harmony" || systemNameFlag == "linux" || systemNameFlag == "freebsd" {
		archiveFiles(debugArchive, archivePrefix, []string{getAppName() + ".dbg"})
		dbgPaths = append(dbgPaths, APPNAME+".dbg")
	} else if systemNameFlag == "android" {
		archiveFiles(debugArchive, archivePrefix, []string{getAppName() + ".dbg"})
		dbgPaths = append(dbgPaths, APPNAME+".dbg")
	} else if systemNameFlag == "darwin" {
		archiveFiles(debugArchive, archivePrefix, []string{getAppName() + ".dSYM"})
	} else if systemNameFlag == "ios" {
		cmdRun([]string{"rm", "-rf", getAppName() + ".dSYM"}, true)
		buildSubdir := cmakeBuildTypeFlag + "-iphoneos"
		if subSystemNameFlag == "simulator" {
			buildSubdir = cmakeBuildTypeFlag + "-iphonesimulator"
		}
		cmdRun([]string{"rm", "-rf", getAppName() + ".dSYM"}, true)
		cmdRun([]string{"rm", "-rf", "YassPacketTunnel.appex.dSYM"}, true)
		cmdRun([]string{"cp", "-r", filepath.Join(buildSubdir, getAppName() + ".dSYM"), getAppName() + ".dSYM"}, true)
		cmdRun([]string{"cp", "-r", filepath.Join(buildSubdir, "YassPacketTunnel.appex.dSYM"), "YassPacketTunnel.appex.dSYM"}, true)
		archiveFiles(debugArchive, archivePrefix, []string{getAppName() + ".dSYM", "YassPacketTunnel.appex.dSYM"})
		dbgPaths = append(dbgPaths, getAppName()+".dSYM", "YassPacketTunnel.appex.dSYM")
	}
	archives[debugArchive] = dbgPaths

	// Create openwrt Makefile
	if subSystemNameFlag == "openwrt" && variantFlag == "cli" {
		generateOpenWrtMakefile(archive, archiveSuffix)
	}
	return archives
}

func get7zPath() string {
	if runtime.GOOS == "windows" {
		return "C:\\Program Files\\7-Zip\\7z.exe"
	} else {
		return "7z"
	}
}

func inspectArchive(file string, files []string) {
	if strings.HasSuffix(file, ".dmg") {
		cmdRun([]string{"hdiutil", "imageinfo", file}, false)
	} else if strings.HasSuffix(file, ".zip") || strings.HasSuffix(file, ".msi") || strings.HasSuffix(file, ".exe") || strings.HasSuffix(file, ".apk") || strings.HasSuffix(file, ".ipa") {
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
	if noConfigureFlag || noBuildFlag {
		return
	}
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
