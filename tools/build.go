package main

import (
	"archive/zip"
	"bufio"
	"encoding/base64"
	"errors"
	"flag"
	"fmt"
	"io"
	"io/fs"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"

	"github.com/golang/glog"
)

var APPNAME string = "yass"

var toolDir string
var projectDir string
var buildDir string

var dryRunFlag bool
var preCleanFlag bool
var noPackagingFlag bool

var cmakeBuildTypeFlag string
var cmakeBuildConcurrencyFlag int

var useLibCxxFlag bool

var clangTidyModeFlag bool
var clangTidyExecutablePathFlag string

var macosxVersionMinFlag string
var macosxUniversalBuildFlag bool
var macosxCodeSignIdentityFlag string

var msvcTargetArchFlag string
var msvcCrtLinkageFlag string
var msvcAllowXpFlag bool

var systemNameFlag string
var sysrootFlag string
var archFlag string

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

	var flagNoPreClean bool

	flag.BoolVar(&dryRunFlag, "dry-run", false, "Generate build script but without actually running it")
	flag.BoolVar(&preCleanFlag, "pre-clean", true, "Clean the source tree before building")
	flag.BoolVar(&flagNoPreClean, "nc", false, "Don't Clean the source tree before building")
	flag.BoolVar(&noPackagingFlag, "no-packaging", false, "Skip packaging step")

	flag.StringVar(&cmakeBuildTypeFlag, "cmake-build-type", getEnv("BUILD_TYPE", "Release"), "Set cmake build configuration")
	flag.IntVar(&cmakeBuildConcurrencyFlag, "cmake-build-concurrency", runtime.NumCPU(), "Set cmake build concurrency")

	flag.BoolVar(&useLibCxxFlag, "use-libcxx", true, "Use Custom libc++")

	flag.BoolVar(&clangTidyModeFlag, "clang-tidy-mode", getEnvBool("ENABLE_CLANG_TIDY", false), "Enable Clang Tidy Build")
	flag.StringVar(&clangTidyExecutablePathFlag, "clang-tidy-executable-path", getEnv("ENABLE_CLANG_TIDY", ""), "Path to clang-tidy, only used by Clang Tidy Build")

	flag.StringVar(&macosxVersionMinFlag, "macosx-version-min", getEnv("MACOSX_VERSION_MIN", "10.10"), "Set Mac OS X deployment target, such as 10.9")
	flag.BoolVar(&macosxUniversalBuildFlag, "macosx-universal-build", getEnvBool("ENABLE_OSX_UNIVERSAL_BUILD", true), "Enable Mac OS X Universal Build")
	flag.StringVar(&macosxCodeSignIdentityFlag, "macosx-codesign-identity", getEnv("CODESIGN_IDENTITY", "-"), "Set Mac OS X CodeSign Identity")

	flag.StringVar(&msvcTargetArchFlag, "msvc-tgt-arch", getEnv("VSCMD_ARG_TGT_ARCH", ""), "Set Visual C++ Target Achitecture")
	flag.StringVar(&msvcCrtLinkageFlag, "msvc-crt-linkage", getEnv("MSVC_CRT_LINKAGE", ""), "Set Visual C++ CRT Linkage")
	flag.BoolVar(&msvcAllowXpFlag, "msvc-allow-xp", getEnvBool("MSVC_ALLOW_XP", false), "Enable Windows XP Build")

	flag.StringVar(&systemNameFlag, "system", runtime.GOOS, "Specify host system name")
	flag.StringVar(&sysrootFlag, "sysroot", "", "Specify host sysroot, used in cross-compiling")
	flag.StringVar(&archFlag, "arch", runtime.GOARCH, "Specify host architecture")

	flag.Parse()

	if flagNoPreClean {
		preCleanFlag = false
	}

	// clang-tidy complains about parse error
	if clangTidyModeFlag {
		macosxUniversalBuildFlag = false
		noPackagingFlag = true
	}
}

func prebuildFindSourceDirectory() {
	glog.Info("PreStage -- Find Source Directory")
	glog.Info("======================================================================")
	os.Chdir(projectDir)
	if _, err := os.Stat("CMakeLists.txt"); errors.Is(err, os.ErrNotExist) {
		glog.Fatalf("Cannot find top dir of the source tree")
	}

	if msvcTargetArchFlag != "" {
		buildDir = fmt.Sprintf("build-msvc-%s-%s", msvcTargetArchFlag, msvcCrtLinkageFlag)
	} else {

		buildDir = fmt.Sprintf("build-%s-%s", systemNameFlag, archFlag)
	}
	var err error
	buildDir, err = filepath.Abs(buildDir)
	if err != nil {
		glog.Fatalf("%v", err)
	}

	glog.V(2).Infof("Set build directory %s", buildDir)
	if _, err := os.Stat(buildDir); err == nil {
		if preCleanFlag {
			os.RemoveAll(buildDir)
			glog.V(2).Infof("Removed previous build directory %s", buildDir)
		}
	}
	if _, err := os.Stat(buildDir); errors.Is(err, os.ErrNotExist) {
		os.Mkdir(buildDir, 0777)
	}
	glog.V(1).Infof("Changed to build directory %s", buildDir)
	os.Chdir(buildDir)
}

func getLLVMTargetTripleMSVC(msvcTargetArch string) string {
	if msvcTargetArch == "x86" {
		return "i686-pc-windows-msvc"
	} else if msvcTargetArch == "x64" {
		return "x86_64-pc-windows-msvc"
	} else if msvcTargetArch == "arm" {
		// lld-link doesn"t accept this triple
		// cmake_args.extend(["-DCMAKE_LINKER=link"])
		return "arm-pc-windows-msvc"
	} else if msvcTargetArch == "arm64" {
		return "arm64-pc-windows-msvc"
	}
	glog.Fatalf("Invalid msvcTargetArch: %s", msvcTargetArch)
	return ""
}

func getGNUTargetTypeAndArch(arch string) (string, string) {
	if arch == "amd64" || arch == "x86_64" {
		return "x86_64-linux-gnu", "x86_64"
	} else if arch == "x86" || arch == "i386" {
		return "i386-linux-gnu", "i386"
	} else if arch == "arm64" || arch == "aarch64" {
		return "aarch64-linux-gnu", "aarch64"
	} else if arch == "armel" {
		return "arm-linux-gnueabi", "armel"
	} else if arch == "arm" {
		return "arm-linux-gnueabihf", "armhf"
	} else if arch == "mips" {
		return "mipsel-linux-gnu", "mipsel"
	} else if arch == "mips64el" {
		return "mips64el-linux-gnuabi64", "mips64el"
	}
	glog.Fatalf("Invalid arch: %s", arch)
	return "", ""
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
	var stdouterr, stdouterrPipe *os.File
	var err error
	stdouterr, stdouterrPipe, err = cmdStdoutStderrPipe(cmd)
	if err != nil {
		if !check {
			return
		}
		glog.Fatalf("%v", err)
	}
	err = cmd.Start()
	stdouterrPipe.Close()
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
		fmt.Println(scanner.Text())
	}
	err = cmd.Wait()
	if err != nil {
		glog.Warningf("Command \"%v\" failed with %v", args, err)
		if !check {
			return
		}
		glog.Fatalf("%v", err)
	}
	stdouterr.Close()
}

func cmdCheckoutput(args []string) string {
	var cmd = exec.Command(args[0], args[1:]...)
	glog.V(2).Infof("Running command \"%v\"", args)
	output, err := cmd.Output()
	if err != nil {
		panic(err)
	}
	return string(output)
}

func buildStageGenerateBuildScript() {
	glog.Info("BuildStage -- Generate Build Script")
	glog.Info("======================================================================")
	cmakeArgs := []string{}
	cmakeArgs = append(cmakeArgs, "-DGUI=ON", "-DCLI=ON", "-DSERVER=ON")
	cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_BUILD_TYPE=%s", cmakeBuildTypeFlag))
	cmakeArgs = append(cmakeArgs, "-G", "Ninja")
	cmakeArgs = append(cmakeArgs, "-DUSE_HOST_TOOLS=on")
	if useLibCxxFlag {
		cmakeArgs = append(cmakeArgs, "-DUSE_LIBCXX=on")
	} else {
		cmakeArgs = append(cmakeArgs, "-DUSE_LIBCXX=off")
	}
	if clangTidyModeFlag {
		cmakeArgs = append(cmakeArgs, "-DENABLE_CLANG_TIDY=on", fmt.Sprintf("-DCLANG_TIDY_EXECUTABLE=%s", clangTidyExecutablePathFlag))
	}
	if systemNameFlag == "windows" {
		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCROSS_TOOLCHAIN_FLAGS_NATIVE=\"-DCMAKE_TOOLCHAIN_FILE=%s\\Native.cmake\"", buildDir))
		// Guesting native LIB paths from host LIB paths
		nativeLibPaths := []string{}
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
		nativeToolChainContent := strings.Replace(fmt.Sprintf("set(CMAKE_C_COMPILER \"%s\")\n", getEnv("CC", "cl")), "\\", "/", -1)
		nativeToolChainContent += strings.Replace(fmt.Sprintf("set(CMAKE_CXX_COMPILER \"%s\")\n", getEnv("CXX", "cl")), "\\", "/", -1)
		nativeToolChainContent += "set(CMAKE_C_COMPILER_TARGET \"x86_64-pc-windows-msvc\")\n"
		nativeToolChainContent += "set(CMAKE_CXX_COMPILER_TARGET \"x86_64-pc-windows-msvc\")\n"
		err := os.WriteFile("Native.cmake", []byte(nativeToolChainContent), 0666)
		if err != nil {
			glog.Fatalf("%v", err)
		}

		cmakeArgs = append(cmakeArgs, fmt.Sprintf("-DCMAKE_MSVC_CRT_LINKAGE=%s", msvcCrtLinkageFlag))
		if msvcAllowXpFlag {
			cmakeArgs = append(cmakeArgs, "-DALLOW_XP=ON")
		} else {
			cmakeArgs = append(cmakeArgs, "-DALLOW_XP=OFF")
		}
		// Some compilers are inherently cross compilers, such as Clang and the QNX QCC compiler.
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

	if systemNameFlag == "linux" {
		gnuType, gnuArch := getGNUTargetTypeAndArch(archFlag)
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
	}

	if systemNameFlag == "freebsd" {
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
	ninjaCmd := []string{"ninja", APPNAME, "-j", fmt.Sprintf("%d", cmakeBuildConcurrencyFlag)}
	cmdRun(ninjaCmd, true)
	// FIXME move to cmake (required by Xcode generator)
	if systemNameFlag == "darwin" {
		if _, err := os.Stat(filepath.Join(cmakeBuildTypeFlag, getAppName())); err == nil {
			renameByUnlink(filepath.Join(cmakeBuildTypeFlag, getAppName()), getAppName())
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
	if systemNameFlag == "linux" || systemNameFlag == "freebsd" {
		objcopy := filepath.Join("..", "third_party", "llvm-build", "Release+Asserts", "bin", "llvm-objcopy")
		if _, err := os.Stat(objcopy); err == os.ErrNotExist {
			objcopy = "objcopy"
		}
		cmdRun([]string{objcopy}, false)
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
	output := cmdCheckoutput([]string{"lipo", "-archs", binaryFile})
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

		walkFunc := func(path string, info fs.FileInfo, err error) error {
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
		filepath.Walk(path, walkFunc)
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
		checkUniveralBuild(getAppName())
	}
}

func copyFile(src string, dst string) error {
	buf := make([]byte, 4096)

	fin, err := os.Open(src)
	if err != nil {
		return err
	}

	defer fin.Close()

	fout, err := os.Create(dst)
	if err != nil {
		return err
	}

	defer fout.Close()

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
	license_maps := map[string]string{
		"LICENSE":            filepath.Join("..", "LICENSE"),
		"LICENSE.abseil-cpp": filepath.Join("..", "third_party", "abseil-cpp", "LICENSE"),
		"LICENSE.asio":       filepath.Join("..", "third_party", "asio", "asio", "LICENSE_1_0.txt"),
		"LICENSE.boringssl":  filepath.Join("..", "third_party", "boringssl", "src", "LICENSE"),
		"LICENSE.chromium":   filepath.Join("..", "third_party", "chromium", "LICENSE"),
		"LICENSE.icu":        filepath.Join("..", "third_party", "icu", "LICENSE"),
		"LICENSE.lss":        filepath.Join("..", "third_party", "lss", "LICENSE"),
		"LICENSE.mozilla":    filepath.Join("..", "third_party", "mozilla", "LICENSE.txt"),
		"LICENSE.protobuf":   filepath.Join("..", "third_party", "protobuf", "LICENSE"),
		"LICENSE.quiche":     filepath.Join("..", "third_party", "quiche", "src", "LICENSE"),
		"LICENSE.rapidjson":  filepath.Join("..", "third_party", "rapidjson", "license.txt"),
		"LICENSE.xxhash":     filepath.Join("..", "third_party", "xxhash", "LICENSE"),
		"LICENSE.zlib":       filepath.Join("..", "third_party", "zlib", "LICENSE"),
	}
	licenses := []string{}
	for license := range license_maps {
		if err := copyFile(license_maps[license], license); err != nil {
			panic(err)
		}
		licenses = append(licenses, license)
	}
	return licenses
}

// add to zip writer
func archiveFileToZip(zipWriter *zip.Writer, info fs.FileInfo, path string) error {
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
	defer f.Close()
	w, err := zipWriter.Create(path)
	if err != nil {
		return err
	}
	if _, err := io.Copy(w, f); err != nil {
		return err
	}
	return nil
}

// generate tgz (posix) or zip file
func archiveFiles(output string, paths []string) {
	if strings.HasSuffix(output, ".tgz") {
		glog.Infof("generating tgz file %s", output)
		cmd := []string{"tar", "cfz", output}
		cmd = append(cmd, paths...)
		cmdRun(cmd, true)
		return
	}
	glog.Infof("generating zip file %s", output)
	archive, err := os.Create(output)
	if err != nil {
		panic(err)
	}
	defer archive.Close()
	zipWriter := zip.NewWriter(archive)
	for _, path := range paths {
		info, err := os.Stat(path)
		if err != nil {
			panic(err)
		}
		if !info.IsDir() {
			err := archiveFileToZip(zipWriter, info, path)
			if err != nil {
				panic(err)
			}
			continue
		}

		walkFunc := func(path string, info fs.FileInfo, err error) error {
			if err != nil {
				return err
			}
			if !info.IsDir() {
				err := archiveFileToZip(zipWriter, info, path)
				if err != nil {
					return err
				}
			}
			return nil
		}
		filepath.Walk(path, walkFunc)
	}
	zipWriter.Close()
}

func archiveMainFile(output string, paths []string) {
	if systemNameFlag == "darwin" {
		var eulaRtf []byte
		eulaTemplate, err := ioutil.ReadFile("../src/mac/eula.xml")
		if err != nil {
			panic(err)
		}
		eulaRtf, err = ioutil.ReadFile("../GPL-2.0.rtf")
		if err != nil {
			panic(err)
		}

		eulaXml := strings.Replace(string(eulaTemplate), "%PLACEHOLDER%", base64.StdEncoding.EncodeToString([]byte(eulaRtf)), -1)

		err = ioutil.WriteFile("eula.xml", []byte(eulaXml), 0666)
		if err != nil {
			panic(err)
		}

		// Use this command line to update .DS_Store
		// hdiutil convert -format UDRW -o yass.dmg yass-macos-release-universal-*.dmg
		// hdiutil resize -size 1G yass.dmg
		cmdCheckoutput([]string{"../scripts/pkg-dmg",
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
		archiveFiles(output, paths)
	}
}

// TBD
func generateMsi(output string, paths []string, dllPaths []string, licensePaths []string) {
}

func postStateArchives() map[string][]string {
	glog.Info("PostState -- Archives")
	glog.Info("======================================================================")
	tag := strings.TrimSpace(cmdCheckoutput([]string{"git", "describe", "--tags", "HEAD"}))
	var archiveFormat string
	if systemNameFlag == "windows" {
		os := "win"
		if msvcAllowXpFlag {
			os = "winxp"
		}
		archiveFormat = fmt.Sprintf("%%s-%s-release-%s-%s-%s%%s%%s", os, msvcTargetArchFlag, msvcCrtLinkageFlag, tag)
	} else if systemNameFlag == "darwin" {
		os := "macos"
		arch := archFlag
		if macosxUniversalBuildFlag {
			arch = "universal"
		}
		archiveFormat = fmt.Sprintf("%%s-%s-release-%s-%s%%s%%s", os, arch, tag)
	} else {
		archiveFormat = fmt.Sprintf("%%s-%s-release-%s-%s%%s%%s", systemNameFlag, archFlag, tag)
	}

	ext := ".zip"

	if systemNameFlag == "linux" || systemNameFlag == "freebsd" {
		ext = ".tgz"
	}

	archive := fmt.Sprintf(archiveFormat, APPNAME, "", ext)
	if systemNameFlag == "darwin" {
		archive = fmt.Sprintf(archiveFormat, APPNAME, "", ".dmg")
	}

	msiArchive := fmt.Sprintf(archiveFormat, APPNAME, "", ".msi")
	debugArchive := fmt.Sprintf(archiveFormat, APPNAME, "-debuginfo", ext)

	archives := map[string][]string{}

	paths := []string{getAppName()}
	dllPaths := []string{}
	dbgPaths := []string{}

	// copying dependent LICENSEs
	licensePaths := postStateArchiveLicenses()
	paths = append(paths, licensePaths...)

	// main bundle
	archiveMainFile(archive, paths)
	archives[archive] = paths

	// msi installer
	if systemNameFlag == "windows" {
		generateMsi(msiArchive, paths, dllPaths, licensePaths)
		archives[msiArchive] = []string{msiArchive}
	}
	// debuginfo file
	if systemNameFlag == "windows" {
		archiveFiles(debugArchive, []string{APPNAME + ".pdb"})
		dbgPaths = append(dbgPaths, APPNAME + ".pdb")
	} else if systemNameFlag == "linux" || systemNameFlag == "freebsd" {
		archiveFiles(debugArchive, []string{APPNAME + ".dbg"})
		dbgPaths = append(dbgPaths, APPNAME + ".dbg")
	} else if systemNameFlag == "darwin" {
		archiveFiles(debugArchive, []string{getAppName() + ".dSYM"})
		dbgPaths = append(dbgPaths, getAppName() + ".dSYM")
	}
	archives[debugArchive] = dbgPaths
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
	} else if strings.HasSuffix(file, ".zip") || strings.HasSuffix(file, ".msi") {
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
	glog.Infof("Hello, World! %s-%s", systemNameFlag, archFlag)
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
