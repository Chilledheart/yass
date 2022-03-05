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

dry_run = None

cmake_build_type = None
cmake_build_concurrency = None

use_libcxx = None

clang_tidy_mode = None
clang_tidy_executable_path = None

macosx_version_min = None
macosx_universal_build = None
macosx_codesign_identity = None

msvc_tgt_arch = None
msvc_crt_linkage = None
msvc_allow_xp = None

system_name = None
sysroot = None
arch = None

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


def write_output(command, check=False):
  print('--- %s' % ' '.join(command))
  if dry_run:
    return
  proc = None
  try:
    proc = subprocess.Popen(command, stdout=sys.stdout, stderr=sys.stderr,
        shell=False, env=os.environ)
  except:
    if check:
      raise
    else:
      return
  try:
    proc.communicate()
  except KeyboardInterrupt:
    proc.kill()
    # We don't call process.wait() as .__exit__ does that for us.
    raise
  except:
    proc.kill()
    # We don't call process.wait() as .__exit__ does that for us.
    if check:
      raise
  retcode = proc.poll()
  if check and retcode:
    raise subprocess.CalledProcessError(retcode, proc.args,
                                        output=sys.stdout, stderr=sys.stderr)

def rename_by_unlink(src, dst):
  if dry_run:
    print(f'rename {src} -> {dst}')
    return
  if os.path.isdir(dst):
    shutil.rmtree(dst)
  elif os.path.isfile(dst):
    os.unlink(dst)
  os.rename(src, dst)


def get_7z_path():
  if system_name == 'Windows':
    return 'C:\Program Files\\7-Zip\\7z.exe'
  else:
    return '7z'


def inspect_file(file, files):
  if file.endswith('.dmg'):
    write_output(['hdiutil', 'imageinfo', file])
  elif file.endswith('.zip') or file.endswith('.msi'):
    p7z = get_7z_path()
    write_output([p7z, 'l', file])
  elif file.endswith('.tgz'):
    write_output(['tar', 'tvf', file])
  else:
    for file in files:
      print('------------ %s' % file)


def get_app_name():
  if system_name == 'Windows':
    return '%s.exe' % APP_NAME
  elif system_name == 'Darwin':
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
  # FIXME skip for now
  if sysroot:
    return []
  lines = subprocess.check_output(['ldd', path]).decode().split('\n')
  outputs = []

  for line in lines:
    if not line.startswith('\t'):
        continue
    if not '=>' in line:
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
    # skip system libraries, part4
    if name.startswith('/usr/local/lib'):
        continue
    # if file not found
    if not os.path.exists(name):
        raise IOError('broken dependency: file %s not found' % name)
    outputs.append(os.path.normpath(name))

  # remove duplicate items
  return list(set(outputs))


def _get_win32_search_paths():
  # no search when using libc++,
  # TODO test under shaded libc++
  if use_libcxx:
    return []

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
  elif vctools_version >= 14.20 and vctools_version < 14.30:
    vcredist_dir = os.path.join(os.getenv('VCINSTALLDIR'), 'Redist', 'MSVC',
                                os.getenv('VCToolsVersion'))
    # fallback
    if not os.path.exists(vcredist_dir):
      vcredist_dir = os.path.join(os.getenv('VCINSTALLDIR'), 'Redist', 'MSVC',
                                  '14.29.30133')
  elif vctools_version >= 14.10 and vctools_version < 14.20:
    vcredist_dir = os.path.join(os.getenv('VCINSTALLDIR'), 'Redist', 'MSVC',
                                os.getenv('VCToolsVersion'))
    # fallback
    if not os.path.exists(vcredist_dir):
      vcredist_dir = os.path.join(os.getenv('VCINSTALLDIR'), 'Redist', 'MSVC',
                                  '14.16.27012')
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
    os.path.join(vcredist_dir, 'debug_nonredist', msvc_tgt_arch,
                 'Microsoft.VC%s.DebugMFC' % platform_toolchain_version),
    os.path.join(vcredist_dir, 'debug_nonredist', msvc_tgt_arch,
                 'Microsoft.VC%s.DebugCRT' % platform_toolchain_version),
    os.path.join(vcredist_dir, msvc_tgt_arch,
                 'Microsoft.VC%s.MFCLOC' % platform_toolchain_version),
    os.path.join(vcredist_dir, msvc_tgt_arch,
                 'Microsoft.VC%s.MFC' % platform_toolchain_version),
    os.path.join(vcredist_dir, msvc_tgt_arch,
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
    os.path.join(sdk_base_dir, 'bin', sdk_version, msvc_tgt_arch, 'ucrt'),
    os.path.join(sdk_base_dir, 'Redist', sdk_version, 'ucrt', 'DLLS',
                 msvc_tgt_arch),
    os.path.join(sdk_base_dir, 'ExtensionSDKs', 'Microsoft.UniversalCRT.Debug',
                 sdk_version, 'Redist', 'Debug', msvc_tgt_arch),
  ])

  ### Fallback search path for XP (v140_xp, v141_xp)
  ### Refer to #27, https://github.com/Chilledheart/yass/issues/27
  ### $project_root\third_party\vcredist\x86
  if vctools_version >= 14.00 and vctools_version < 14.20:
    search_dirs.extend([
      os.path.abspath(os.path.join('..', 'third_party', 'vcredist', msvc_tgt_arch))
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
                 'MSIMG32.dll', 'WINMM.dll', 'CRYPT32.dll', 'gdiplus.dll',
                 'COMCTL32.dll']

  p = re.compile(r'    (\S+.dll)', re.IGNORECASE)
  for line in lines:
    m = p.match(line)
    if m and not m[1] in system_dlls:
      dlls.append(m[1])

  resolved_dlls = []
  unresolved_dlls = []

  # handle MSVC Runtime and UCRT
  if msvc_crt_linkage == 'dynamic' and not use_libcxx:
    # this library is not included directly but required
    # ideas comes from https://source.chromium.org/chromium/chromium/src/+/main:build/win/BUILD.gn?q=ucrtbase.dll
    if cmake_build_type == 'Debug':
      vcrt_suffix = 'd'
    else:
      vcrt_suffix = ''
    dlls.append(f'msvcp140{vcrt_suffix}.dll')
    dlls.append(f'msvcp140_1{vcrt_suffix}.dll')
    dlls.append(f'msvcp140_2{vcrt_suffix}.dll')
    dlls.append(f'vcruntime140{vcrt_suffix}.dll')

    # Fix for visual studio 2019 redist
    vctools_version_str = os.getenv('VCToolsVersion')
    if len(vctools_version_str) >= 4:
      # for vc141 or above, VCToolsVersion=14.??.xxxx
      vctools_version = float(vctools_version_str[:5])
    else:
      # for vc140 or below, VCToolsVersion=14.0
      vctools_version = float(vctools_version_str)

    if vctools_version >= 14.20:
      dlls.append(f'msvcp140{vcrt_suffix}_atomic_wait.dll')
      dlls.append(f'msvcp140{vcrt_suffix}_codecvt_ids.dll')
      # In x64 builds there is an extra C runtime DLL called vcruntime140_1.dll (and it's debug equivalent).
      if msvc_tgt_arch == 'x64' or msvc_tgt_arch == 'arm64':
        dlls.append(f'vcruntime140_1{vcrt_suffix}.dll')

    if msvc_tgt_arch != 'arm64':
      dlls.extend([
        # Universal Windows 10 CRT files
        "api-ms-win-core-console-l1-1-0.dll",
        "api-ms-win-core-datetime-l1-1-0.dll",
        "api-ms-win-core-debug-l1-1-0.dll",
        "api-ms-win-core-errorhandling-l1-1-0.dll",
        "api-ms-win-core-file-l1-1-0.dll",
        "api-ms-win-core-file-l1-2-0.dll",
        "api-ms-win-core-file-l2-1-0.dll",
        "api-ms-win-core-handle-l1-1-0.dll",
        "api-ms-win-core-heap-l1-1-0.dll",
        "api-ms-win-core-interlocked-l1-1-0.dll",
        "api-ms-win-core-libraryloader-l1-1-0.dll",
        "api-ms-win-core-localization-l1-2-0.dll",
        "api-ms-win-core-memory-l1-1-0.dll",
        "api-ms-win-core-namedpipe-l1-1-0.dll",
        "api-ms-win-core-processenvironment-l1-1-0.dll",
        "api-ms-win-core-processthreads-l1-1-0.dll",
        "api-ms-win-core-processthreads-l1-1-1.dll",
        "api-ms-win-core-profile-l1-1-0.dll",
        "api-ms-win-core-rtlsupport-l1-1-0.dll",
        "api-ms-win-core-string-l1-1-0.dll",
        "api-ms-win-core-synch-l1-1-0.dll",
        "api-ms-win-core-synch-l1-2-0.dll",
        "api-ms-win-core-sysinfo-l1-1-0.dll",
        "api-ms-win-core-timezone-l1-1-0.dll",
        "api-ms-win-core-util-l1-1-0.dll",
        "api-ms-win-crt-conio-l1-1-0.dll",
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
        "api-ms-win-crt-utility-l1-1-0.dll",
        "ucrtbase.dll",
      ])

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
  if system_name == 'Windows':
    return get_dependencies_by_dumpbin(path, _get_win32_search_paths())[0]
  elif system_name == 'Darwin':
    return get_dependencies_by_otool(path)
  elif system_name == 'Linux' or system_name == 'FreeBSD':
    return get_dependencies_by_ldd(path)
  else:
    raise IOError('not supported in platform %s' % system_name)


def get_dependencies_recursively(path):
  if system_name == 'Windows':
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

  elif system_name == 'Darwin':
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
  elif system_name == 'Linux' or system_name == 'FreeBSD':
    return get_dependencies_by_ldd(path)
  else:
    raise IOError('not supported in platform %s' % system_name)


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
    write_output(['patchelf', '--set-rpath', '$ORIGIN/lib', binrary], check=True)
  libs = os.listdir(lib_path)
  for lib_name in libs:
    lib = os.path.join(lib_path, lib_name)
    if os.path.isdir(lib):
      continue
    write_output(['patchelf', '--set-rpath', '$ORIGIN/lib', lib], check=True)


def prebuild_find_source_directory(pre_clean):
  print('find source directory...')
  os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
  if not os.path.exists('CMakeLists.txt'):
    print('Please execute this script under the top dir of the source tree')
    sys.exit(-1)
  build_dir = f'build-msvc-{msvc_tgt_arch}-{msvc_crt_linkage}' if msvc_tgt_arch else f'build-{system_name}-{arch}'
  if os.path.exists(build_dir):
    if pre_clean:
      shutil.rmtree(build_dir)
      sleep(1)
  if not os.path.exists(build_dir):
    os.mkdir(build_dir)
  os.chdir(build_dir)


def build_stage_generate_build_script():
  print('generate build scripts...(%s)' % cmake_build_type)
  cmake_args = ['-DGUI=ON', '-DCLI=ON', '-DSERVER=ON']
  cmake_args.extend(['-DCMAKE_BUILD_TYPE=%s' % cmake_build_type])
  cmake_args.extend(['-G', 'Ninja'])
  cmake_args.extend(['-DUSE_HOST_TOOLS=on'])
  if use_libcxx:
    cmake_args.extend(['-DUSE_LIBCXX=on'])
  else:
    cmake_args.extend(['-DUSE_LIBCXX=off'])
  if clang_tidy_mode:
    cmake_args.extend(['-DENABLE_CLANG_TIDY=yes',
                       '-DCLANG_TIDY_EXECUTABLE=%s' % clang_tidy_executable_path])
  if system_name == 'Windows':
    cmake_args.extend(['-DCROSS_TOOLCHAIN_FLAGS_NATIVE="-DCMAKE_TOOLCHAIN_FILE=%s\\Native.cmake"' % os.getcwd()])
    native_libs = []
    for native_lib in os.getenv('LIB').split(';'):
      # Old Windows SDK looks like:
      # C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Lib
      # C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Lib\x64
      if 'v7.1a' in native_lib.lower():
        if native_lib.lower().endswith('lib'):
          p = os.path.join(native_lib, 'x64')
        else:
          p = native_lib
      else:
        p = os.path.join(os.path.dirname(native_lib), 'x64')
      native_libs.append(p.replace('\\', '/'))
    NATIVE_LIB = ';'.join(native_libs)
    # override link's LIBS in parent scope
    cmake_args.extend([f'-DCROSS_TOOLCHAIN_FLAGS_NATIVE_LIB="{NATIVE_LIB}"'])
    with open('Native.cmake', 'w') as f:
      CC = os.getenv('CC', 'cl')
      CXX = os.getenv('CXX', 'cl')
      f.write(f'set(CMAKE_C_COMPILER "{CC}")\n'.replace('\\', '/'))
      f.write(f'set(CMAKE_CXX_COMPILER "{CXX}")\n'.replace('\\', '/'))
      f.write('set(CMAKE_C_COMPILER_TARGET "x86_64-pc-windows-msvc")\n')
      f.write('set(CMAKE_CXX_COMPILER_TARGET "x86_64-pc-windows-msvc")\n')
    if msvc_crt_linkage == 'static':
      cmake_args.extend(['-DCMAKE_MSVC_CRT_LINKAGE=static'])
    else:
      cmake_args.extend(['-DCMAKE_MSVC_CRT_LINKAGE=dynamic'])
    if msvc_allow_xp:
      cmake_args.extend(['-DALLOW_XP=ON'])

    # Some compilers are inherently cross compilers, such as Clang and the QNX QCC compiler.
    # The CMAKE_<LANG>_COMPILER_TARGET can be set to pass a value to those supported compilers when compiling.
    # see https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_TARGET.html
    if msvc_tgt_arch == 'x86':
      llvm_triple = 'i686-pc-windows-msvc'
    elif msvc_tgt_arch == 'x64':
      llvm_triple = 'x86_64-pc-windows-msvc'
    elif msvc_tgt_arch == 'arm':
      llvm_triple = 'arm-pc-windows-msvc'
      # lld-link doesn't accept this triple
      cmake_args.extend(['-DCMAKE_LINKER=link'])
    elif msvc_tgt_arch == 'arm64':
      llvm_triple = 'arm64-pc-windows-msvc'
    if 'clang-cl' in os.getenv('CC', '') and llvm_triple:
      cmake_args.extend(['-DCMAKE_C_COMPILER_TARGET=%s' % llvm_triple])
      cmake_args.extend(['-DCMAKE_CXX_COMPILER_TARGET=%s' % llvm_triple])
    if msvc_tgt_arch == 'arm' or msvc_tgt_arch == 'arm64':
      cmake_args.extend(['-DCMAKE_ASM_FLAGS=--target=%s' % llvm_triple])

  if system_name == 'Darwin':
    cmake_args.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=%s' % macosx_version_min)
    if macosx_universal_build:
      cmake_args.append('-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64')

  if system_name == 'Linux' and sysroot:
    if arch == 'amd64' or arch == 'x86_64':
      target = 'x86_64-linux-gnu'
      processor = 'x86_64'
    elif arch == 'x86' or arch == 'i386':
      target = 'i386-linux-gnu'
      processor = 'x86'
    elif arch == 'arm64' or arch == 'aarch64':
      target = 'aarch64-linux-gnu'
      processor = 'aarch64'
    elif arch == 'armel':
      target = 'arm-linux-gnueabi'
      processor = 'armel'
    elif arch == 'arm':
      target = 'arm-linux-gnueabihf'
      processor = 'armhf'
    elif arch == 'mips':
      target = 'mipsel-linux-gnu'
      processor = 'mipsel'
    elif arch == 'mips64el':
      target = 'mips64el-linux-gnuabi64'
      processor = 'mips64el'
    else:
      raise Exception('Unsupported arch %s' % arch)

    toolchain_file = os.path.realpath('%s/../cmake/platforms/Linux.cmake' % os.getcwd())
    cmake_args.extend(['-DCMAKE_TOOLCHAIN_FILE=%s' % toolchain_file])
    os.environ['PKG_CONFIG_PATH'] = os.path.join(sysroot, 'usr', 'lib', 'pkgconfig')
    os.environ['PKG_CONFIG_PATH'] += ';' + os.path.join(sysroot, 'usr', 'share', 'pkgconfig')
    cmake_args.extend(['-DLLVM_SYSROOT=%s/../third_party/llvm-build/Release+Asserts' % os.getcwd()])
    cmake_args.extend(['-DGCC_SYSROOT=%s' % sysroot])
    cmake_args.extend(['-DGCC_SYSTEM_PROCESSOR=%s' % processor])
    cmake_args.extend(['-DGCC_TARGET=%s' % target])

  if system_name == 'FreeBSD' and sysroot:
    if arch == 'amd64' or arch == 'x86_64':
      target = 'x86_64-freebsd'
      processor = 'x86_64'
    elif arch == 'x86' or arch == 'i386':
      target = 'i386-freebsd'
      processor = 'x86'
    elif arch == 'arm64' or arch == 'aarch64':
      target = 'aarch64-freebsd'
      processor = 'aarch64'
    else:
      raise Exception('Unsupported arch %s' % arch)

    toolchain_file = os.path.realpath('%s/../cmake/platforms/FreeBSD.cmake' % os.getcwd())
    cmake_args.extend(['-DCMAKE_TOOLCHAIN_FILE=%s' % toolchain_file])
    os.environ['PKG_CONFIG_PATH'] = os.path.join(sysroot, 'usr', 'libdata', 'pkgconfig')
    os.environ['PKG_CONFIG_PATH'] += ';' + os.path.join(sysroot, 'usr', 'local', 'libdata', 'pkgconfig')
    cmake_args.extend(['-DLLVM_SYSROOT=%s/../third_party/llvm-build/Release+Asserts' % os.getcwd()])
    cmake_args.extend(['-DGCC_SYSROOT=%s' % sysroot])
    cmake_args.extend(['-DGCC_SYSTEM_PROCESSOR=%s' % processor])
    cmake_args.extend(['-DGCC_TARGET=%s' % target])

  command = ['cmake', '..'] + cmake_args

  write_output(command, check=True)


def build_stage_execute_buildscript():
  print('executing build scripts...(%s)' % cmake_build_type)

  command = ['ninja', APP_NAME, '-j', cmake_build_concurrency]
  write_output(command, check=True)
  # FIX ME move to cmake (required by Xcode generator)
  if system_name == 'Darwin':
    if os.path.exists(os.path.join(cmake_build_type, get_app_name())):
      rename_by_unlink(os.path.join(cmake_build_type, get_app_name()), get_app_name())


def postbuild_copy_libraries():
  print('copying dependent libraries...')
  if system_name == 'Windows':
    _postbuild_copy_libraries_win32()
  elif system_name == 'Darwin':
    _postbuild_copy_libraries_xcode()
  else:
    _postbuild_copy_libraries_posix()


def postbuild_fix_rpath():
  print('fixing rpath...')
  if system_name == 'Windows':
    # 'no need to fix rpath'
    pass
  elif system_name == 'Linux' or system_name == 'FreeBSD':
    # Skip for now as we don't have depedents libraries
    #_postbuild_patchelf()
    pass
  elif system_name == 'Darwin':
    _postbuild_install_name_tool()
  else:
    print('not supported in platform %s' % system_name)


def postbuild_strip_binaries():
  print('strip binaries...')
  if system_name == 'Windows':
    # no need to strip?
    pass
  elif system_name == 'Linux' or system_name == 'FreeBSD':
    objcopy = os.path.join('..', 'third_party', 'llvm-build', 'Release+Asserts', 'bin', 'llvm-objcopy')
    if not os.path.exists(objcopy):
      objcopy = 'objcopy'
    # create a file containing the debugging info.
    write_output([objcopy, '--only-keep-debug', APP_NAME, APP_NAME + '.dbg'])
    # stripped executable.
    write_output([objcopy, '--strip-debug', APP_NAME])
    # to add a link to the debugging info into the stripped executable.
    write_output([objcopy, '--add-gnu-debuglink=' + APP_NAME + '.dbg', APP_NAME])
  elif system_name == 'Darwin':
    write_output(['dsymutil',
                  os.path.join(get_app_name(), 'Contents', 'MacOS', APP_NAME),
                  '--statistics',
                  '--papertrail',
                  '-o', get_app_name() + '.dSYM'])
    write_output(['strip', '-S', '-x', '-v', os.path.join(get_app_name(), 'Contents', 'MacOS', APP_NAME)])
  else:
    print('not supported in platform %s' % system_name)


def postbuild_codesign():
  print('codesign...')
  # reference https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution/resolving_common_notarization_issues?language=objc
  # Hardened runtime is available in the Capabilities pane of Xcode 10 or later
  write_output(['codesign', '--timestamp=none',
                '--preserve-metadata=entitlements', '--options=runtime',
                '--force', '--deep', '--sign', macosx_codesign_identity,
                get_app_name()], check=True)
  write_output(['codesign', '-dv', '--deep', '--strict', '--verbose=4',
                get_app_name()], check=True)
  write_output(['codesign', '-d', '--entitlements', ':-', get_app_name()],
               check=True)


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


# generate tgz (posix) or zip file
def archive_files(output, paths = []):
  if output.endswith('.tgz'):
    print(f'generating tgz file {output}')
    cmd = ['tar', 'cvfz', output]
    cmd.extend(paths)
    write_output(cmd, check=True)
    return

  from zipfile import ZipFile, ZIP_DEFLATED, ZipInfo

  print(f'generating zip file {output}')
  with ZipFile(output, 'w', compression=ZIP_DEFLATED, compresslevel=9) as archive:
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


def _archive_files_main_files(output, paths):
  if system_name == 'Darwin':
    from base64 import b64encode
    with open('../src/mac/eula.xml', 'r') as f:
      eula_template = f.read()
    with open('../GPL-2.0.rtf', 'r') as f:
      eula_rtf = f.read()

    eula_xml = eula_template.replace('%PLACEHOLDER%', b64encode(eula_rtf.encode('utf-8')).decode())

    with open('eula.xml', 'w') as f:
      f.write(eula_xml)

    # Use this command line to update .DS_Store
    # hdiutil convert -format UDRW -o yass.dmg yass-macos-release-universal-*.dmg
    # hdiutil resize -size 1G yass.dmg
    write_output(['../scripts/pkg-dmg', '--source', paths[0], '--target', output,
                  '--sourcefile',
                  '--volname', 'Yet Another Shadow Socket',
                  '--resource', 'eula.xml',
                  '--icon', '../src/mac/yass.icns',
                  '--copy', '../macos/.DS_Store:/.DS_Store',
                  '--copy', '../macos/.background:/',
                  '--symlink', '/Applications:/Applications'],
                 check=True)
  else:
    archive_files(output, paths)


def _archive_files_dll_files():
  dll_paths = []
  if system_name == 'Windows':
    files = os.listdir('.')
    for file in files:
      if file.endswith('.dll'):
        dll_paths.append(file)
  return dll_paths


def _archive_files_license_files():
  # LICENSEs
  license_maps = {
    'LICENSE': os.path.join('..', 'LICENSE'),
    'LICENSE.abseil-cpp': os.path.join('..', 'third_party', 'abseil-cpp', 'LICENSE'),
    'LICENSE.asio': os.path.join('..', 'third_party', 'asio', 'asio', 'LICENSE_1_0.txt'),
    'LICENSE.boringssl': os.path.join('..', 'third_party', 'boringssl', 'src', 'LICENSE'),
    'LICENSE.chromium': os.path.join('..', 'third_party', 'chromium', 'LICENSE'),
    'LICENSE.icu': os.path.join('..', 'third_party', 'icu', 'LICENSE'),
    'LICENSE.lss': os.path.join('..', 'third_party', 'lss', 'LICENSE'),
    'LICENSE.mozilla': os.path.join('..', 'third_party', 'mozilla', 'LICENSE.txt'),
    'LICENSE.protobuf': os.path.join('..', 'third_party', 'protobuf', 'LICENSE'),
    'LICENSE.quiche': os.path.join('..', 'third_party', 'quiche', 'src', 'LICENSE'),
    'LICENSE.rapidjson': os.path.join('..', 'third_party', 'rapidjson', 'license.txt'),
    'LICENSE.xxhash': os.path.join('..', 'third_party', 'xxhash', 'LICENSE'),
    'LICENSE.zlib': os.path.join('..', 'third_party', 'zlib', 'LICENSE'),
  }
  licenses = []
  for license in license_maps:
    shutil.copyfile(license_maps[license], license)
    licenses.append(license)
  return licenses


def _get_guid():
  from uuid import uuid4
  return str(uuid4()).upper()


def _archive_files_generate_msi(msi_archive, paths, dll_paths, license_paths):
  from xml.dom import minidom, Node

  print('Generating WiX source file...')
  wxs_content = minidom.parse('../yass.wxs')

  # update product id
  wxs_content.getElementsByTagName('Product')[0].setAttribute('Id', _get_guid())

  # update dll components and licenses components
  components = wxs_content.getElementsByTagName('Component')
  for comp in components:
    if comp.getAttribute('Id') == 'SupportLibrary':
      keyPath = True
      for dll in dll_paths:
        for subcomp in comp.childNodes:
          if subcomp.nodeType == Node.COMMENT_NODE:
            comp.removeChild(subcomp)
        new_dll = wxs_content.createElement('File')
        new_dll.setAttribute('Name', os.path.basename(dll))
        new_dll.setAttribute('Source', dll)
        new_dll.setAttribute('KeyPath', 'yes' if keyPath else 'no')
        keyPath = False
        comp.appendChild(new_dll)
    elif comp.getAttribute('Id') == 'License':
      keyPath = True
      for license in license_paths:
        for subcomp in comp.childNodes:
          if subcomp.nodeType == Node.COMMENT_NODE:
            comp.removeChild(subcomp)
        new_license = wxs_content.createElement('File')
        new_license.setAttribute('Name', os.path.basename(license))
        new_license.setAttribute('Source', license)
        new_license.setAttribute('KeyPath', 'yes' if keyPath else 'no')
        keyPath = False
        comp.appendChild(new_license)

  with open('yass.wxs', 'w') as f:
    f.write(wxs_content.toxml())

  print('Feeding WiX compiler...')
  write_output(['candle.exe', 'yass.wxs'], check=True)

  print('Generating MSI file...')
  write_output(['light.exe', '-ext', 'WixUIExtension', '-out', msi_archive,
                '-cultures:en-US', '-sice:ICE03', '-sice:ICE57', '-sice:ICE61', 'yass.wixobj'], check=True)


def postbuild_archive():
  src = get_app_name()
  dst = APP_NAME

  archive = dst + '.zip' if system_name != 'Darwin' else dst + '.dmg'
  msi_archive = 'yass.msi'
  debuginfo_archive = dst + '-debuginfo.zip'

  if system_name == 'Linux' or system_name == 'FreeBSD':
    archive = archive.replace('.zip', '.tgz')
    debuginfo_archive = debuginfo_archive.replace('.zip', '.tgz')

  archives = {}

  paths = [ src ]
  dll_paths = []
  license_paths = []
  debuginfo_paths = []

  # copying dependent dlls
  dll_paths = _archive_files_dll_files()

  # copying dependent LICENSEs
  license_paths = _archive_files_license_files()

  paths.extend(dll_paths)
  paths.extend(license_paths)

  # main bundle
  _archive_files_main_files(archive, paths)
  archives[archive] = paths

  # msi installer
  if system_name == 'Windows':
    _archive_files_generate_msi(msi_archive, paths, dll_paths, license_paths)
    archives[msi_archive] = [msi_archive]

  # debuginfo file
  if system_name == 'Windows':
    if os.path.exists(APP_NAME + '.pdb'):
      debuginfo_paths.append(APP_NAME + '.pdb')
      archive_files(debuginfo_archive, debuginfo_paths)
      archives[debuginfo_archive] = debuginfo_paths
  elif system_name == 'Linux' or system_name == 'FreeBSD':
    if os.path.exists(APP_NAME + '.dbg'):
      debuginfo_paths.append(APP_NAME + '.dbg')
      archive_files(debuginfo_archive, debuginfo_paths)
      archives[debuginfo_archive] = debuginfo_paths
  elif system_name == 'Darwin':
    if os.path.exists(get_app_name() + '.dSYM'):
      debuginfo_paths.append(get_app_name() + '.dSYM')
      archive_files(debuginfo_archive, debuginfo_paths)
      archives[debuginfo_archive] = debuginfo_paths

  return archives

def postbuild_rename_archive(archives):
  renamed_archives = {}
  t = check_string_output(['git', 'describe', '--tags', 'HEAD']);
  for archive in archives:
    main, ext = os.path.splitext(archive)
    suffix = main[len(APP_NAME):]
    main = APP_NAME

    if system_name == 'Windows':
      a = msvc_tgt_arch
      l = msvc_crt_linkage
      xp = 'xp' if msvc_allow_xp else ''
      renamed_archive = f'{main}-win{xp}-release-{a}-{l}-{t}{suffix}{ext}'
    elif system_name == 'Darwin':
      a = 'universal'
      renamed_archive = f'{main}-macos-release-{a}-{t}{suffix}{ext}'
    elif system_name == 'Linux':
      a = arch.lower()
      renamed_archive = f'{main}-linux-release-{a}-{t}{suffix}{ext}'
    else:
      s = system_name.lower()
      a = arch.lower()
      renamed_archive = f'{main}-{s}-release-{t}{suffix}{ext}'

    renamed_archive = os.path.join('..', renamed_archive)

    rename_by_unlink(archive, renamed_archive)
    renamed_archives[renamed_archive] = archives[archive]
  return renamed_archives

def main():
  import argparse

  global APP_NAME

  global dry_run

  global cmake_build_type
  global cmake_build_concurrency

  global use_libcxx

  global macosx_version_min
  global macosx_universal_build
  global macosx_codesign_identity

  global msvc_tgt_arch
  global msvc_crt_linkage
  global msvc_allow_xp

  global clang_tidy_mode
  global clang_tidy_executable_path

  global system_name
  global sysroot
  global arch

  parser = argparse.ArgumentParser()

  parser.add_argument('--dry-run', help='Generate build script but without actually running it',
                      action='store_true', default=False)

  group = parser.add_mutually_exclusive_group()
  group.add_argument('-nc', '--no-pre-clean', help='Don\'t Clean the source tree before building',
                     action='store_true')
  group.add_argument('--pre-clean', help='Clean the source tree before building',
                     default=True, action='store_true')

  parser.add_argument('-np', '--no-packaging', help='Don\'t do packaging',
                      default=False, action='store_true')

  parser.add_argument('--cmake-build-type', help='Set cmake build configuration',
                      choices=['Debug', 'Release'],
                      default=os.getenv('BUILD_TYPE', 'Release'))
  parser.add_argument('--cmake-build-concurrency', help='Set cmake build concurrency',
                      type=int, default=os.getenv('NCPUS', cpu_count()))

  group = parser.add_mutually_exclusive_group()
  group.add_argument('--use-libcxx', help='Use Custom libc++',
                     default=os.getenv('USE_LIBCXX', True),
                     action='store_true')
  group.add_argument('--no-use-libcxx', help='Don\'t use custom libcxx',
                     action='store_true')

  parser.add_argument('--macosx-version-min', help='Set Mac OS X deployment target, such as 10.9',
                      default=os.getenv('MACOSX_VERSION_MIN', '10.10'))
  parser.add_argument('--macosx-universal-build', help='Mac OS X Universal Build',
                      default=os.getenv('ENABLE_OSX_UNIVERSAL_BUILD', True),
                      action='store_true')
  parser.add_argument('--macosx-codesign-identity', help='Set Mac OS X CodeSign Identity',
                      default=os.getenv('CODESIGN_IDENTITY', '-'))

  parser.add_argument('--msvc-tgt-arch', help='Set Visual C++ Target Achitecture',
                      choices=['x86', 'arm', 'x64', 'arm64'],
                      default=os.getenv('VSCMD_ARG_TGT_ARCH'))
  parser.add_argument('--msvc-crt-linkage', help='Set Visual C++ CRT Linkage',
                      choices=['dynamic', 'static'],
                      default=os.getenv('MSVC_CRT_LINKAGE', 'static'))
  parser.add_argument('--msvc-allow-xp', help='Windows XP Build',
                      default=os.getenv('MSVC_ALLOW_XP', False),
                      action='store_true')

  parser.add_argument('--clang-tidy-mode', help='Enable Clang Tidy Build',
                      default=os.getenv('ENABLE_CLANG_TIDY', False),
                      action='store_true')
  parser.add_argument('--clang-tidy-executable-path', help='Path to clang-tidy, only used by Clang Tidy Build',
                      default=os.getenv('CLANG_TIDY_EXECUTABLE', 'clang-tidy'))

  parser.add_argument('--system', help='Specify host name',
                      choices=['Windows', 'FreeBSD', 'Linux', 'Darwin'], default=platform.system())
  parser.add_argument('--sysroot', help='Specify host sysroot, used in cross-compiling',
                      default='')
  parser.add_argument('--arch', help='Specify host arch',
                      choices=['x86', 'x86_64', 'amd64', 'arm', 'arm64',
                               'armel', 'i386', 'mips', 'mips64el', 'aarch64'],
                      default=platform.machine())

  args = parser.parse_args()

  dry_run = args.dry_run

  cmake_build_type = args.cmake_build_type
  cmake_build_concurrency = str(args.cmake_build_concurrency)

  use_libcxx = False if args.no_use_libcxx else args.use_libcxx

  macosx_version_min = args.macosx_version_min
  macosx_universal_build = args.macosx_universal_build
  macosx_codesign_identity = args.macosx_codesign_identity

  msvc_tgt_arch = args.msvc_tgt_arch
  msvc_crt_linkage = args.msvc_crt_linkage
  msvc_allow_xp = args.msvc_allow_xp

  clang_tidy_mode = args.clang_tidy_mode
  clang_tidy_executable_path = args.clang_tidy_executable_path

  system_name = args.system
  sysroot = args.sysroot
  arch = args.arch

  # clang-tidy complains about parse error
  if clang_tidy_mode:
    macosx_universal_build = False

  pre_clean = False if args.no_pre_clean else args.pre_clean
  skip_packaging = True if args.clang_tidy_mode else args.no_packaging

  prebuild_find_source_directory(pre_clean)

  print('======================================================================')

  build_stage_generate_build_script()

  print('======================================================================')

  build_stage_execute_buildscript()

  print('======================================================================')

  if skip_packaging:
    print('done')
    sys.exit(0)

  postbuild_copy_libraries()

  print('======================================================================')

  if system_name != 'Windows':
    postbuild_fix_rpath()
    print('======================================================================')

  postbuild_strip_binaries()

  print('======================================================================')

  if cmake_build_type == 'Release' and system_name == 'Darwin':
    postbuild_codesign()
    print('======================================================================')

  if macosx_universal_build and system_name == 'Darwin':
    postbuild_check_universal_build()
    print('======================================================================')

  archives = postbuild_archive()
  print('======================================================================')

  archives = postbuild_rename_archive(archives)
  print('======================================================================')

  for archive in archives:
    print('------ %s:' % os.path.basename(archive))
    print('======================================================================')
    inspect_file(archive, archives[archive])

  print('done')

if __name__ == '__main__':
  print("")
  print("  *******************************************************")
  print("  Deprecated: please use build tool under tools directory")
  print("  *******************************************************")
  print("")
  main()
