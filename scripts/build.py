#!/usr/bin/env python3
import os
import re
import shutil
import subprocess
import sys
import platform
from multiprocessing import cpu_count
from time import sleep

APP_NAME = 'yass'
# configurable variable are Debug, Release, RelWithDebInfo and MinSizeRel
DEFAULT_BUILD_TYPE = os.getenv('BUILD_TYPE', 'Release')
DEFAULT_OSX_MIN = os.getenv('MACOSX_VERSION_MIN', '10.10')
# enable by default if macports installed
DEFAULT_ENABLE_OSX_UNIVERSAL_BUILD = os.getenv('ENABLE_OSX_UNIVERSAL_BUILD',
                                               True)
DEFAULT_OSX_UNIVERSAL_ARCHS = 'arm64;x86_64'
DEFAULT_ARCH = os.getenv('VSCMD_ARG_TGT_ARCH', 'x86')
# configurable variable are static and dynamic
DEFAULT_MSVC_CRT_LINKAGE = os.getenv('MSVC_CRT_LINKAGE', 'dynmaic')
DEFAULT_ALLOW_XP = os.getenv('ALLOW_XP', False)
DEFAULT_SIGNING_IDENTITY = os.getenv('CODESIGN_IDENTITY', '-')
DEFAULT_ENABLE_CLANG_TIDY = os.getenv('ENABLE_CLANG_TIDY', False)
DEFAULT_CLANG_TIDY_EXECUTABLE = os.getenv('CLANG_TIDY_EXECUTABLE', 'clang-tidy')
# documented in github actions https://github.com/actions/virtual-environments/blob/main/images/win/Windows2019-Readme.md
# and VCPKG_ROOT is used for compatible mode
VCPKG_DIR = os.getenv('VCPKG_INSTALLATION_ROOT', os.getenv('VCPKG_ROOT', 'C:\vcpkg'))

# clang-tidy complains about parse error
if DEFAULT_ENABLE_CLANG_TIDY:
  DEFAULT_ENABLE_OSX_UNIVERSAL_BUILD = False

num_cpus=cpu_count()

def copy_file_with_symlinks(src, dst):
  if os.path.lexists(dst):
    os.unlink(dst)
  if os.path.islink(src):
    linkto = os.readlink(src)
    os.symlink(linkto, dst)
    return
  shutil.copyfile(src, dst)


def check_string_output(command):
    return subprocess.check_output(command, stderr=subprocess.STDOUT).decode().strip()


def write_output(command, suppress_error=True):
  print('--- %s' % ' '.join(command))
  proc = subprocess.Popen(command, stdout=sys.stdout, stderr=sys.stdout,
      shell=False, env=os.environ)
  proc.communicate()
  if not suppress_error and proc.returncode != 0:
    raise RuntimeError('cmd "%s" failed, exit code %d' % (' '.join(command),
                                                          proc.returncode))


def get_app_name():
  if sys.platform == 'win32':
    return '%s.exe' % APP_NAME
  elif sys.platform == 'darwin':
    return '%s.app' % APP_NAME
  return APP_NAME

def get_dependencies_by_otool(path):
  if path.endswith('.app') and os.path.isdir(path):
    path = os.path.join(path, 'Contents', 'MacOS', os.path.basename(path))
  lines = subprocess.check_output(['otool', '-L', path]).decode().split('\n')[1:]
  outputs = []
  if path.endswith('.dylib'):
    outputs.append(path);

  for line in lines:
    name = line.split('(')[0].strip()
    if name.startswith('@executable_path/'):
      name = os.path.join(path, name[len('@executable_path/'):])
    if name.startswith('@loader_path/'):
      name = os.path.join(path, name[len('@loader_path/'):])
    # todo: search rpath
    if not name.startswith('/'):
      continue
    # skip system libraries
    if name.startswith('/usr/lib') or name.startswith('/System'):
      continue
    # if file not found
    if not os.path.exists(name):
      raise IOError('broken dependency: file %s not found' % name)
    outputs.append(os.path.normpath(name))

  return outputs


def get_dependencies_by_ldd(path):
  lines = subprocess.check_output(['ldd', '-v', path]).decode().split('Version information:')[1].split('\n\t')
  outputs = []

  for line in lines:
    if not line.startswith('\t'):
        continue
    name = line.strip().split('=>')[1].strip()
    # todo: search rpath
    if name.startswith('$ORIGIN/'):
        name = os.path.join(path, name[len('$ORIGIN/'):])
    # skip system libraries, such as linux-vdso.so
    if not name.startswith('/'):
        continue
    # skip system libraries, part2
    if name.startswith('/usr/lib') or name.startswith('/usr/lib64'):
        continue
    # skip system libraries, part3
    if name.startswith('/lib') or name.startswith('/lib64'):
        continue
    # if file not found
    if not os.path.exists(name):
        raise IOError('broken dependency: file %s not found' % name)
    outputs.append(os.path.normpath(name))

  # remove duplicate items
  return list(set(outputs))


def get_dependencies_by_objdump(path):
  print('todo')
  return []


def _get_win32_search_paths():
  # VCToolsVersion:PlatformToolchainversion:VisualStudioVersion
  #  14.30-14.3?:v143:Visual Studio 2022
  #  14.20-14.29:v142:Visual Studio 2019
  #  14.10-14.19:v141:Visual Studio 2017
  #  14.00-14.00:v140:Visual Studio 2015
  #
  #  From wiki: https://en.wikipedia.org/wiki/Microsoft_Visual_C%2B%2B
  #  https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=msvc-170#microsoft-specific-predefined-macros
  #  6.00    Visual Studio 6.0                          1200
  #  7.00    Visual Studio .NET 2002 (7.0)              1300
  #  7.10    Visual Studio .NET 2003 (7.1)              1310
  #  8.00    Visual Studio 2005 (8.0)                   1400
  #  9.00    Visual Studio 2008 (9.0)                   1500
  #  10.00   Visual Studio 2010 (10.0)                  1600
  #  12.00   Visual Studio 2013 (12.0)                  1800
  #  14.00   Visual Studio 2015 (14.0)                  1900
  #  14.10   Visual Studio 2017 RTW (15.0)              1910
  #  14.11   Visual Studio 2017 version 15.3            1911
  #  14.12   Visual Studio 2017 version 15.5            1912
  #  14.13   Visual Studio 2017 version 15.6            1913
  #  14.14   Visual Studio 2017 version 15.7            1914
  #  14.15   Visual Studio 2017 version 15.8            1915
  #  14.16   Visual Studio 2017 version 15.9            1916
  #  14.20   Visual Studio 2019 RTW (16.0)              1920
  #  14.21   Visual Studio 2019 version 16.1            1921
  #  14.22   Visual Studio 2019 version 16.2            1922
  #  14.23   Visual Studio 2019 version 16.3            1923
  #  14.24   Visual Studio 2019 version 16.4            1924
  #  14.25   Visual Studio 2019 version 16.5            1925
  #  14.26   Visual Studio 2019 version 16.6            1926
  #  14.27   Visual Studio 2019 version 16.7            1927
  #  14.28   Visual Studio 2019 version 16.8, 16.9      1928
  #  14.29   Visual Studio 2019 version 16.10, 16.11    1929
  #  14.30   Visual Studio 2022 RTW (17.0)              1930
  #  14.31   Visual Studio 2022 version 17.1            1931
  #
  #  Visual Studio 2015 is not supported by this script due to
  #  the missing environment variable VCToolsVersion
  vctools_version_str = os.getenv('VCToolsVersion')
  if len(vctools_version_str) >= 4:
    # for vc141 or above, VCToolsVersion=14.??.xxxx
    vctools_version = float(vctools_version_str[:5])
  else:
    # for vc140 or below, VCToolsVersion=14.0
    vctools_version = float(vctools_version_str)

  if vctools_version >= 14.30:
    platform_toolchain_version = '143'
  elif vctools_version >= 14.20 and vctools_version < 14.30:
    platform_toolchain_version = '142'
  elif vctools_version >= 14.10 and vctools_version < 14.20:
    platform_toolchain_version = '141'
  elif vctools_version >= 14.00 and vctools_version < 14.10:
    platform_toolchain_version = '140'
  else:
    raise RuntimeError('unsupported vctoolchain version %s' % vctools_version)

  # Environment variable VCToolsRedistDir isn't correct when it doesn't match
  # Visual Studio's default toolchain.
  # according to microsoft's design issue, don't use it.
  #
  # for example:
  #
  # VCINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\
  # VCToolsInstallDir=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.16.27023\
  # VCToolsRedistDir=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\14.30.30704\
  #
  # for vc140 it's:
  # C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\redist
  if vctools_version >= 14.30:
    vcredist_dir = os.getenv('VCToolsRedistDir')
  elif vctools_version >= 14.10 and vctools_version < 14.30:
    vcredist_dir = os.path.join(os.getenv('VCINSTALLDIR'), 'Redist', 'MSVC',
                                os.getenv('VCToolsVersion'))
  elif vctools_version >= 14.00 and vctools_version < 14.10:
    vcredist_dir = os.path.join(os.getenv('VCINSTALLDIR'), 'redist')
  else:
    vcredist_dir = ""

  ### Search Path (VC Runtime and MFC) (vctools_version newer than v140)
  ### C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\14.30.30704\debug_nonredist\x86\Microsoft.VC143.DebugMFC
  ### C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\14.30.30704\debug_nonredist\x86\Microsoft.VC143.DebugCRT
  ### C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\14.30.30704\x86\Microsoft.VC143.MFCLOC
  ### C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\14.30.30704\x86\Microsoft.VC143.MFC
  ### C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\14.30.30704\x86\Microsoft.VC143.CRT
  search_dirs = [
    os.path.join(vcredist_dir, 'debug_nonredist', DEFAULT_ARCH,
                 'Microsoft.VC%s.DebugMFC' % platform_toolchain_version),
    os.path.join(vcredist_dir, 'debug_nonredist', DEFAULT_ARCH,
                 'Microsoft.VC%s.DebugCRT' % platform_toolchain_version),
    os.path.join(vcredist_dir, DEFAULT_ARCH,
                 'Microsoft.VC%s.MFCLOC' % platform_toolchain_version),
    os.path.join(vcredist_dir, DEFAULT_ARCH,
                 'Microsoft.VC%s.MFC' % platform_toolchain_version),
    os.path.join(vcredist_dir, DEFAULT_ARCH,
                 'Microsoft.VC%s.CRT' % platform_toolchain_version),
  ]
  # remove the trailing slash
  sdk_version = os.getenv('WindowsSDKVersion')[:-1]
  sdk_base_dir= os.getenv('WindowsSdkDir')

  ### Search Path (UCRT)
  ###
  ### Please note The UCRT files are not redistributable for ARM64 Win32.
  ### https://chromium.googlesource.com/chromium/src/+/lkgr/build/win/BUILD.gn
  ### C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x86\ucrt
  ### C:\Program Files (x86)\Windows Kits\10\Redist\10.0.19041.0\ucrt\DLLS\x86
  ### C:\Program Files (x86)\Windows Kits\10\ExtensionSDKs\Microsoft.UniversalCRT.Debug\10.0.19041.0\Redist\Debug\x86
  search_dirs.extend([
    os.path.join(sdk_base_dir, 'bin', sdk_version, DEFAULT_ARCH, 'ucrt'),
    os.path.join(sdk_base_dir, 'Redist', sdk_version, 'ucrt', 'DLLS',
                 DEFAULT_ARCH),
    os.path.join(sdk_base_dir, 'ExtensionSDKs', 'Microsoft.UniversalCRT.Debug',
                 sdk_version, 'Redist', 'Debug', DEFAULT_ARCH),
  ])

  ### Fallback search path for XP (v140)
  ### Refer to #27, https://github.com/Chilledheart/yass/issues/27
  ### $project_root\third_party\vcredist\x86
  if vctools_version >= 14.00 and vctools_version < 14.10:
    search_dirs.extend([
      os.path.abspath(os.path.join('..', 'third_party', 'vcredist', DEFAULT_ARCH))
    ])
  return search_dirs

"""
The output of dumpbin /dependents is like below:
Microsoft (R) COFF/PE Dumper Version 14.30.30709.0
Copyright (C) Microsoft Corporation.  All rights reserved.


Dump of file arm64-windows-yass.exe

File Type: EXECUTABLE IMAGE

  Image has the following dependencies:

    WS2_32.dll
    GDI32.dll
    SHELL32.dll
    KERNEL32.dll
    USER32.dll
    ADVAPI32.dll
    MSVCP140.dll
    MSWSOCK.dll
    mfc140u.dll
    dbghelp.dll
    VCRUNTIME140.dll
    api-ms-win-crt-runtime-l1-1-0.dll
    api-ms-win-crt-heap-l1-1-0.dll
    api-ms-win-crt-stdio-l1-1-0.dll
    api-ms-win-crt-math-l1-1-0.dll
    api-ms-win-crt-time-l1-1-0.dll
    api-ms-win-crt-filesystem-l1-1-0.dll
    api-ms-win-crt-convert-l1-1-0.dll
    api-ms-win-crt-environment-l1-1-0.dll
    api-ms-win-crt-string-l1-1-0.dll
    api-ms-win-crt-locale-l1-1-0.dll

  Summary

       15000 .data
        4000 .pdata
       2C000 .rdata
        2000 .reloc
       12000 .rsrc
       71000 .text
"""
def get_dependencies_by_dumpbin(path, search_dirs):
  lines = subprocess.check_output(['dumpbin', '/dependents', path]).decode().split('\n')
  dlls = []
  system_dlls = ['WS2_32.dll', 'GDI32.dll', 'SHELL32.dll', 'USER32.dll',
                 'ADVAPI32.dll', 'MSWSOCK.dll', 'dbghelp.dll', 'KERNEL32.dll',
                 'ole32.dll', 'OLEAUT32.dll', 'SHLWAPI.dll', 'IMM32.dll',
                 'UxTheme.dll', 'PROPSYS.dll', 'dwmapi.dll', 'WININET.dll',
                 'OLEACC.dll', 'ODBC32.dll', 'oledlg.dll', 'urlmon.dll',
                 'MSIMG32.dll', 'WINMM.dll', 'CRYPT32.dll', 'gdiplus.dll', ]

  p = re.compile(r'    (\S+.dll)', re.IGNORECASE)
  for line in lines:
    m = p.match(line)
    if m and not m[1] in system_dlls:
      dlls.append(m[1])

  resolved_dlls = []
  unresolved_dlls = []
  for dll in dlls:
    resolved = False
    for search_dir in search_dirs:
      d = os.path.join(search_dir, dll)
      if os.path.exists(d):
        resolved_dlls.append(d)
        resolved = True
        break
    if not resolved:
      unresolved_dlls.append(dll)

  return resolved_dlls, unresolved_dlls


def get_dependencies(path):
  if sys.platform == 'win32':
    return get_dependencies_by_dumpbin(path, _get_win32_search_paths())[0]
  elif sys.platform == 'darwin':
    return get_dependencies_by_otool(path)
  elif sys.platform in [ 'linux', 'linux2' ]:
    return get_dependencies_by_ldd(path)
  else:
    raise IOError('not supported in platform %s' % sys.platform)


def get_dependencies_recursively(path):
  if sys.platform == 'win32':
    search_dirs = _get_win32_search_paths()
    print('searching dlls in directories:')
    for search_dir in search_dirs:
      print('--- %s' % search_dir)
    deps, unresolved_deps = get_dependencies_by_dumpbin(path, search_dirs)
    while(True):
        deps_extended = list(deps)
        for dep in deps:
            deps_deps, deps_unresolved_deps = get_dependencies_by_dumpbin(dep, search_dirs)
            deps_extended.extend(deps_deps)
            unresolved_deps.extend(deps_unresolved_deps)
        deps_extended = set(deps_extended)
        if deps_extended != deps:
            deps = deps_extended;
        else:
            break
    unresolved_deps = list(set(unresolved_deps))
    unresolved_deps.sort()
    print('missing depended dlls:')
    for unresolved_dep in unresolved_deps:
      print('--- %s' % unresolved_dep)
    return list(deps)

  elif sys.platform == 'darwin':
    deps = get_dependencies_by_otool(path)
    while(True):
        deps_extended = list(deps)
        for dep in deps:
            deps_extended.extend(get_dependencies_by_otool(dep))
        deps_extended = set(deps_extended)
        if deps_extended != deps:
            deps = deps_extended;
        else:
            return list(deps_extended)
  elif sys.platform in [ 'linux', 'linux2' ]:
    return get_dependencies_by_ldd(path)
  else:
    raise IOError('not supported in platform %s' % sys.platform)


def check_universal_fat_bundle_darwin(bundle_path):
  main_file = os.path.join(bundle_path, 'Contents', 'MacOS',
                           os.path.splitext(os.path.basename(bundle_path))[0])
  # TODO should we check recursilvely?
  return check_universal_fat_file_darwin(main_file)


def check_universal_fat_file_darwin(lib_path):
  arches = check_string_output(['lipo', '-archs', lib_path])
  if 'x86_64' in arches and 'arm64' in arches:
    return True

  return False

def _postbuild_copy_libraries_win32():
  dlls = get_dependencies_recursively(get_app_name())
  # for pretty output
  dlls.sort()
  print('copying depended dll file:')
  for dll in dlls:
    print('--- %s' % dll)
    shutil.copyfile(dll, os.path.basename(dll))

def _postbuild_copy_libraries_posix():
  lib_path = 'lib'
  binaries = [APP_NAME]
  libs = []
  for binary in binaries:
      libs.extend(get_dependencies_recursively(binary))
  if not os.path.isdir(lib_path):
      os.makedirs(lib_path)
  for lib in libs:
      shutil.copyfile(lib, lib_path + '/' + os.path.basename(lib))


def _postbuild_copy_libraries_xcode():
  frameworks_path = os.path.join(get_app_name(), 'Contents', 'Frameworks')
  resources_path = os.path.join(get_app_name(), 'Contents', 'Resources')
  macos_path = os.path.join(get_app_name(), 'Contents', 'MacOS')
  binaries = [os.path.join(macos_path, APP_NAME)]
  libs = []
  for binary in binaries:
    libs.extend(get_dependencies_recursively(binary))
  if not os.path.isdir(frameworks_path):
    os.makedirs(frameworks_path)
  for lib in libs:
    dstname = frameworks_path + '/' + os.path.basename(lib)
    copy_file_with_symlinks(lib, dstname)
    if os.path.islink(lib):
      if os.readlink(lib).startswith('/'):
        lib = os.readlink(lib)
        dstname = frameworks_path + '/' + os.path.basename(lib)
      else:
        lib = os.path.dirname(lib) + '/' + os.readlink(lib)
        dstname = frameworks_path + '/' + os.readlink(dstname)
      copy_file_with_symlinks(lib, dstname)
      if os.path.islink(lib):
        if os.readlink(lib).startswith('/'):
          lib = os.readlink(lib)
          dstname = frameworks_path + '/' + os.path.basename(lib)
        else:
          lib = os.path.dirname(lib) + '/' + os.readlink(lib)
          dstname = frameworks_path + '/' + os.readlink(dstname)
        copy_file_with_symlinks(lib, dstname)


def _postbuild_install_name_tool():
  frameworks_path = os.path.join(get_app_name(), 'Contents', 'Frameworks')
  resources_path = os.path.join(get_app_name(), 'Contents', 'Resources')
  macos_path = os.path.join(get_app_name(), 'Contents', 'MacOS')
  binaries = [os.path.join(macos_path, APP_NAME)]
  for binary in binaries:
    write_output(['install_name_tool', '-add_rpath', '@executable_path/../Frameworks', binary])
    deps = get_dependencies(binary)
    for dep in deps:
      dep_name = os.path.basename(dep)
      write_output(['install_name_tool', '-change', dep, '@executable_path/../Frameworks/%s' % dep_name, binary])
    write_output(['install_name_tool', '-delete_rpath', '/usr/local/lib', binary])
    write_output(['install_name_tool', '-delete_rpath', '/opt/local/lib', binary])
  libs = os.listdir(frameworks_path)
  for lib_name in libs:
    lib = os.path.join(frameworks_path, lib_name)
    if os.path.isdir(lib):
      continue
    write_output(['install_name_tool', '-id', '@loader_path/../Frameworks/%s' % os.path.basename(lib), lib])
    write_output(['install_name_tool', '-add_rpath', '@loader_path/../Frameworks', lib])
    write_output(['install_name_tool', '-delete_rpath', '/usr/local/lib', lib])
    write_output(['install_name_tool', '-delete_rpath', '/opt/local/lib', lib])
    deps = get_dependencies(lib)
    for dep in deps:
      dep_name = os.path.basename(dep)
      write_output(['install_name_tool', '-change', dep, '@loader_path/../Frameworks/%s' % dep_name, lib])


def _postbuild_patchelf():
  lib_path = 'lib'
  binaries = [APP_NAME]
  for binrary in binaries:
    if os.path.isdir(binrary):
      continue
    write_output(['patchelf', '--set-rpath', '$ORIGIN/lib', binrary],
                 suppress_error=False)
  libs = os.listdir(lib_path)
  for lib_name in libs:
    lib = os.path.join(lib_path, lib_name)
    if os.path.isdir(lib):
      continue
    write_output(['patchelf', '--set-rpath', '$ORIGIN/lib', lib],
                 suppress_error=False)


def find_source_directory():
  print('find source directory...')
  os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
  if not os.path.exists('CMakeLists.txt'):
    print('Please execute this frome the top dir of the source')
    sys.exit(-1)
  if os.path.exists('build'):
    shutil.rmtree('build')
    sleep(1)
  os.mkdir('build')
  os.chdir('build')


def generate_buildscript(configuration_type):
  print('generate build scripts...(%s)' % configuration_type)
  cmake_args = ['-DGUI=ON', '-DCLI=ON', '-DSERVER=ON']
  if DEFAULT_ENABLE_CLANG_TIDY:
    cmake_args.extend(['-DENABLE_CLANG_TIDY=yes',
                       '-DCLANG_TIDY_EXECUTABLE=%s' % DEFAULT_CLANG_TIDY_EXECUTABLE])
  if sys.platform == 'win32':
    cmake_args.extend(['-G', 'Ninja'])
    cmake_args.extend(['-DCMAKE_BUILD_TYPE=%s' % configuration_type])
    cmake_args.extend(['-DCMAKE_TOOLCHAIN_FILE=%s\\scripts\\buildsystems\\vcpkg.cmake' % VCPKG_DIR])
    cmake_args.extend(['-DVCPKG_TARGET_ARCHITECTURE=%s' % DEFAULT_ARCH])
    if DEFAULT_MSVC_CRT_LINKAGE == 'static':
      cmake_args.extend(['-DCMAKE_MSVC_CRT_LINKAGE=static'])
      cmake_args.extend(['-DVCPKG_CRT_LINKAGE=static'])
      cmake_args.extend(['-DVCPKG_LIBRARY_LINKAGE=static'])
      cmake_args.extend(['-DVCPKG_TARGET_TRIPLET=%s-windows-static' % DEFAULT_ARCH])
    else:
      cmake_args.extend(['-DCMAKE_MSVC_CRT_LINKAGE=dynamic'])
      cmake_args.extend(['-DVCPKG_CRT_LINKAGE=dynamic'])
      cmake_args.extend(['-DVCPKG_LIBRARY_LINKAGE=dynamic'])
      cmake_args.extend(['-DVCPKG_TARGET_TRIPLET=%s-windows' % DEFAULT_ARCH])
    if DEFAULT_ALLOW_XP:
      cmake_args.extend(['-DALLOW_XP=ON'])
    cmake_args.extend(['-DVCPKG_ROOT_DIR=%s' % VCPKG_DIR])
    cmake_args.extend(['-DVCPKG_VERBOSE=ON'])

    # Some compilers are inherently cross compilers, such as Clang and the QNX QCC compiler.
    # The CMAKE_<LANG>_COMPILER_TARGET can be set to pass a value to those supported compilers when compiling.
    # see https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_TARGET.html
    if DEFAULT_ARCH == 'x86':
      llvm_triple = 'i686-pc-windows-msvc'
    elif DEFAULT_ARCH == 'x64':
      llvm_triple = 'x86_64-pc-windows-msvc'
    elif DEFAULT_ARCH == 'arm64':
      llvm_triple = 'aarch64-pc-windows-msvc'
    if 'clang-cl' in os.getenv('CC', '') and llvm_triple:
      cmake_args.extend(['-DCMAKE_C_COMPILER_TARGET=%s' % llvm_triple])
      cmake_args.extend(['-DCMAKE_CXX_COMPILER_TARGET=%s' % llvm_triple])

  else:
    cmake_args.extend(['-G', 'Ninja'])
    cmake_args.extend(['-DCMAKE_BUILD_TYPE=%s' % configuration_type])

  if platform.system() == 'Darwin':
    cmake_args.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=%s' % DEFAULT_OSX_MIN)
    if DEFAULT_ENABLE_OSX_UNIVERSAL_BUILD:
      cmake_args.append('-DCMAKE_OSX_ARCHITECTURES=%s' % DEFAULT_OSX_UNIVERSAL_ARCHS)

  command = ['cmake', '..'] + cmake_args

  write_output(command, suppress_error=False)


def execute_buildscript(configuration_type):
  print('executing build scripts...(%s)' % configuration_type)

  command = ['ninja', 'yass']
  write_output(command, suppress_error=False)
  # FIX ME move to cmake (required by Xcode generator)
  if platform.system() == 'Darwin':
    if os.path.exists(os.path.join(configuration_type, get_app_name())):
      os.rename(os.path.join(configuration_type, get_app_name()), get_app_name())


def postbuild_copy_libraries():
  print('copying dependent libraries...')
  if platform.system() == 'Windows':
    _postbuild_copy_libraries_win32()
  elif platform.system() == 'Darwin':
    _postbuild_copy_libraries_xcode()
  else:
    _postbuild_copy_libraries_posix()


def postbuild_fix_rpath():
  print('fixing rpath...')
  if platform.system() == 'Windows':
    # 'no need to fix rpath'
    pass
  elif platform.system() == 'Linux':
    _postbuild_patchelf()
  elif platform.system() == 'Darwin':
    _postbuild_install_name_tool()
  else:
    print('not supported in platform %s' % platform.system())


def postbuild_strip_binaries():
  print('strip binaries...')
  if platform.system() == 'Windows':
    # no need to strip?
    pass
  elif platform.system() == 'Linux':
    # TODO refer to deb/rpm routine
    pass
  elif platform.system() == 'Darwin':
    write_output(['dsymutil',
                  os.path.join(get_app_name(), 'Contents', 'MacOS', APP_NAME),
                  '--statistics',
                  '--papertrail',
                  '-o', get_app_name() + '.dSYM'])
    write_output(['strip', '-S', '-x', '-v', os.path.join(get_app_name(), 'Contents', 'MacOS', APP_NAME)])
  else:
    print('not supported in platform %s' % platform.system())


def postbuild_codesign():
  print('fixing codesign...')
  # reference https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution/resolving_common_notarization_issues?language=objc
  # Hardened runtime is available in the Capabilities pane of Xcode 10 or later
  write_output(['codesign', '--timestamp=none',
                '--preserve-metadata=entitlements', '--options=runtime',
                '--force', '--deep', '--sign', DEFAULT_SIGNING_IDENTITY,
                get_app_name()], suppress_error=False)
  write_output(['codesign', '-dv', '--deep', '--strict', '--verbose=4',
                get_app_name()], suppress_error=False)
  write_output(['codesign', '-d', '--entitlements', ':-', get_app_name()],
               suppress_error=False)


def _check_universal_build_darwin(path, verbose = False):
  checked = True
  status_msg = ''
  error_msg = ''

  if path.endswith('.framework') or path.endswith('.app') or path.endswith('.appex'):
    if not check_universal_fat_bundle_darwin(path):
      status_msg += 'bundle "%s" is not universal built\n' % path
      error_msg += 'bundle "%s" is not universal built\n' % path
      checked = False
    else:
      status_msg += 'bundle "%s" is universal built\n' % path

    for root, dirs, files in os.walk(path):
      for d in dirs:
        full_path = os.path.join(root, d)
        if full_path.endswith('.framework') or full_path.endswith('.app') or full_path.endswith('.appex'):
          if not check_universal_fat_bundle_darwin(full_path):
            error_msg += 'bundle "%s" is not universal built\n' % full_path
            status_msg += 'bundle "%s" is not universal built\n' % full_path
            checked = False
          else:
            status_msg += 'bundle "%s" is universal built\n' % full_path

      for f in files:
        full_path = os.path.join(root, f)
        if full_path.endswith('.dylib') or full_path.endswith('.so'):
          if not check_universal_fat_file_darwin(full_path):
            error_msg += 'dylib "%s" is not universal built\n' % full_path
            status_msg += 'dylib "%s" is not universal built\n' % full_path
            checked = False
          else:
            status_msg += 'dylib "%s" is universal built\n' % full_path

  if path.endswith('.dylib') or path.endswith('.so'):
    if not check_universal_fat_file_darwin(path):
      error_msg += 'dylib "%s" is not universal built\n' % path
      status_msg += 'dylib "%s" is not universal built\n' % path
      checked = False
    else:
      status_msg += 'dylib "%s" is universal built\n' % path


  if verbose:
    print(status_msg)

  if not checked:
    raise IOError(error_msg)


def postbuild_check_universal_build():
    print('check universal build...')
    # check if binary is built universally
    _check_universal_build_darwin(get_app_name(), verbose = True)


def archive_files(output, paths = []):
  from zipfile import ZipFile, ZIP_DEFLATED, ZipInfo
  with ZipFile(output, 'w', compression=ZIP_DEFLATED) as archive:
    for path in paths:
      archive.write(path, path, ZIP_DEFLATED)
      if os.path.isdir(path):
        for root, dirs, files in os.walk(path):
          for f in files:
            fullPath = os.path.join(root, f)
            if os.path.islink(fullPath):
              # http://www.mail-archive.com/python-list@python.org/msg34223.html
              zipInfo = ZipInfo(fullPath)
              zipInfo.create_system = 3
              # long type of hex val of '0xA1ED0000L',
              # say, symlink attr magic...
              zipInfo.external_attr = 2716663808
              archive.writestr(zipInfo, os.readlink(fullPath))
            else:
              archive.write(fullPath, fullPath, ZIP_DEFLATED)



def postbuild_archive():
  src = get_app_name()
  dst = APP_NAME

  archive = dst + '.zip'
  debuginfo_archive = dst + '-debuginfo.zip'
  new_archive = os.path.join('..', archive)
  new_debuginfo_archive = os.path.join('..', debuginfo_archive)

  paths = [ src ]

  try:
    os.unlink(new_archive)
  except:
    pass

  try:
    os.unlink(new_debuginfo_archive)
  except:
    pass

  # dependent dlls
  if platform.system() == 'Windows':
    files = os.listdir('.')
    for file in files:
      if file.endswith('.dll'):
        paths.append(file)

  # LICENSEs
  shutil.copyfile(os.path.join('..', 'GPL-2.0'), 'LICENSE')
  shutil.copyfile(os.path.join('..', 'third_party', 'abseil-cpp', 'LICENSE'),
                  'LICENSE.abseil-cpp')
  shutil.copyfile(os.path.join('..', 'third_party', 'asio', 'asio', 'LICENSE_1_0.txt'),
                  'LICENSE.asio')
  shutil.copyfile(os.path.join('..', 'third_party', 'boringssl', 'src', 'LICENSE'),
                  'LICENSE.boringssl')
  shutil.copyfile(os.path.join('..', 'third_party', 'lss', 'LICENSE'),
                  'LICENSE.lss')
  shutil.copyfile(os.path.join('..', 'third_party', 'rapidjson', 'license.txt'),
                  'LICENSE.rapidjson')
  shutil.copyfile(os.path.join('..', 'third_party', 'zlib', 'LICENSE'),
                  'LICENSE.zlib')
  shutil.copyfile(os.path.join('..', 'third_party', 'protobuf', 'LICENSE'),
                  'LICENSE.protobuf')
  shutil.copyfile(os.path.join('..', 'third_party', 'xxhash', 'LICENSE'),
                  'LICENSE.xxhash')
  shutil.copyfile(os.path.join('..', 'LICENSE.chromium'), 'LICENSE.chromium')
  paths.append('LICENSE')
  paths.append('LICENSE.abseil-cpp')
  paths.append('LICENSE.asio')
  paths.append('LICENSE.boringssl')
  paths.append('LICENSE.lss')
  paths.append('LICENSE.rapidjson')
  paths.append('LICENSE.zlib')
  paths.append('LICENSE.protobuf')
  paths.append('LICENSE.xxhash')
  paths.append('LICENSE.chromium')

  archive_files(new_archive, paths)

  archives = {}
  archives[archive] = paths

  if platform.system() == 'Windows':
    debuginfo_paths = []

    if os.path.exists(APP_NAME + '.pdb'):
      debuginfo_paths.append(APP_NAME + '.pdb')

    archive_files(new_debuginfo_archive, debuginfo_paths)
    archives[debuginfo_archive] = debuginfo_paths
  elif platform.system() == 'Darwin':
    debuginfo_paths = []
    if os.path.exists(get_app_name() + '.dSYM'):
      debuginfo_paths.append(get_app_name() + '.dSYM')

    archive_files(new_debuginfo_archive, debuginfo_paths)
    archives[debuginfo_archive] = debuginfo_paths

  return archives

if __name__ == '__main__':
  configuration_type = DEFAULT_BUILD_TYPE

  find_source_directory()
  generate_buildscript(configuration_type)
  execute_buildscript(configuration_type)
  if DEFAULT_ENABLE_CLANG_TIDY:
    print('done')
    sys.exit(0)
  postbuild_copy_libraries()
  if platform.system() != 'Windows':
    postbuild_fix_rpath()
  postbuild_strip_binaries()
  if platform.system() == 'Darwin':
    postbuild_codesign()
  if DEFAULT_ENABLE_OSX_UNIVERSAL_BUILD and platform.system() == 'Darwin':
    postbuild_check_universal_build()
  archives = postbuild_archive()
  for archive in archives:
    print('------ %s:' % archive)
    files = archives[archive]
    for file in files:
      print('------------ %s' % file)
  print('done')
