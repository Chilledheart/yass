#!/usr/bin/env python
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
DEFAULT_ENABLE_OSX_UNIVERSAL_BUILD = os.getenv('ENABLE_OSX_UNIVERSAL_BUILD', os.path.exists('/opt/local/bin/port'))
DEFAULT_OSX_UNIVERSAL_ARCHS = 'arm64;x86_64'
DEFAULT_ARCH = os.getenv('VSCMD_ARG_TGT_ARCH', 'x86')
# configurable variable are static and dynamic
DEFAULT_CRT_LINKAGE = 'static'
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
  lines = subprocess.check_output(['otool', '-L', path]).split('\n')[1:]
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
  lines = subprocess.check_output(['ldd', '-v', path]).split('Version information:')[1].split('\n\t')
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


def get_dependencies_by_dependency_walker(path):
  print('todo')
  return []


def get_dependencies(path):
  if sys.platform == 'win32':
    return get_dependencies_by_objdump(path)
  elif sys.platform == 'darwin':
    return get_dependencies_by_otool(path)
  elif sys.platform in [ 'linux', 'linux2' ]:
    return get_dependencies_by_ldd(path)
  else:
    raise IOError('not supported in platform %s' % sys.platform)


def get_dependencies_recursively(path):
  if sys.platform == 'win32':
    return get_dependencies_by_objdump(path)
  elif sys.platform == 'darwin':
    deps = get_dependencies_by_otool(path)
    while(True):
        deps_extened = list(deps)
        for dep in deps:
            deps_extened.extend(get_dependencies_by_otool(dep))
        deps_extened = set(deps_extened)
        if deps_extened != deps:
            deps = deps_extened;
        else:
            return list(deps_extened)
  elif sys.platform in [ 'linux', 'linux2' ]:
    return get_dependencies_by_ldd(path)
  else:
    raise IOError('not supported in platform %s' % sys.platform)


def check_string_output(command):
    return subprocess.check_output(command, stderr=subprocess.STDOUT).decode().strip()


def check_universal_build_bundle_darwin(bundle_path):
  # export from NSBundle.h
  NSBundleExecutableArchitectureX86_64    = 0x01000007
  NSBundleExecutableArchitectureARM64     = 0x0100000c
  from Foundation import NSBundle

  bundle = NSBundle.bundleWithPath_(bundle_path)
  if not bundle:
    return False

  arches = bundle.executableArchitectures()
  if NSBundleExecutableArchitectureX86_64 in arches and NSBundleExecutableArchitectureARM64 in arches:
    return True

  return False


def check_universal_build_dylib_darwin(lib_path):
  arches = check_string_output(['lipo', '-archs', lib_path])
  if 'x86_64' in arches and 'arm64' in arches:
    return True

  return False


def _postbuild_copy_libraries_posix():
  lib_path = 'lib'
  binaries = [APP_NAME]
  libs = []
  for binrary in binaries:
      libs.extend(get_dependencies_recursively(binrary))
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
  for binrary in binaries:
    libs.extend(get_dependencies_recursively(binrary))
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
    if DEFAULT_CRT_LINKAGE == 'static':
      cmake_args.extend(['-DVCPKG_CRT_LINKAGE=static'])
      cmake_args.extend(['-DVCPKG_LIBRARY_LINKAGE=static'])
      cmake_args.extend(['-DVCPKG_TARGET_TRIPLET=%s-windows-static' % DEFAULT_ARCH])
    else:
      cmake_args.extend(['-DVCPKG_CRT_LINKAGE=dynamic'])
      cmake_args.extend(['-DVCPKG_LIBRARY_LINKAGE=dynamic'])
      cmake_args.extend(['-DVCPKG_TARGET_TRIPLET=%s-windows' % DEFAULT_ARCH])
    cmake_args.extend(['-DVCPKG_ROOT_DIR=%s' % VCPKG_DIR])
    cmake_args.extend(['-DVCPKG_VERBOSE=ON'])
    # TODO support boringssl for all arches
    # skip boringssl for platforms except amd64
    if DEFAULT_ARCH == 'x64':
      cmake_args.extend(['-DBORINGSSL=ON'])
    else: #x86, arm64, arm
      cmake_args.extend(['-DBORINGSSL=OFF'])
  else:
    cmake_args.extend(['-G', 'Ninja'])
    cmake_args.extend(['-DCMAKE_BUILD_TYPE=%s' % configuration_type])
    cmake_args.extend(['-DBORINGSSL=ON'])

  if sys.platform == 'darwin':
    cmake_args.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=%s' % DEFAULT_OSX_MIN)
    # if we run arm64/arm64e, use universal build
    if DEFAULT_ENABLE_OSX_UNIVERSAL_BUILD:
      cmake_args.append('-DCMAKE_OSX_ARCHITECTURES=%s' % DEFAULT_OSX_UNIVERSAL_ARCHS)

  command = ['cmake', '..'] + cmake_args

  write_output(command, suppress_error=False)


def execute_buildscript(configuration_type):
  print('executing build scripts...(%s)' % configuration_type)

  command = ['ninja', 'yass']
  write_output(command, suppress_error=False)


def postbuild_copy_libraries():
  print('copying dependent libraries...')
  if sys.platform == 'win32':
    # TODO make windows DLL-vesion build
    # see https://docs.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library?view=msvc-140
    pass
  elif sys.platform == 'darwin':
    _postbuild_copy_libraries_xcode()
  else:
    _postbuild_copy_libraries_posix()


def postbuild_fix_rpath():
  print('fixing rpath...')
  if sys.platform == 'win32':
    # 'no need to fix rpath'
    pass
  elif sys.platform in [ 'linux', 'linux2' ]:
    _postbuild_patchelf()
  elif sys.platform == 'darwin':
    _postbuild_install_name_tool()
  else:
    print('not supported in platform %s' % sys.platform)


def postbuild_fix_retina_display():
  print('fixing retina display...')
  #write_output(['plutil', '-insert', 'LSUIElement', '-bool', 'YES', '%s/Contents/Info.plist' % get_app_name()])
  write_output(['plutil', '-insert', 'NSPrincipalClass', '-string', 'NSApplication', '%s/Contents/Info.plist' % get_app_name()],
               suppress_error=False)
  write_output(['plutil', '-insert', 'NSHighResolutionCapable', '-bool', 'YES', '%s/Contents/Info.plist' % get_app_name()],
               suppress_error=False)


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
    if not check_universal_build_bundle_darwin(path):
      status_msg += 'bundle "%s" is not universal built\n' % path
      error_msg += 'bundle "%s" is not universal built\n' % path
      checked = False
    else:
      status_msg += 'bundle "%s" is universal built\n' % path

    for root, dirs, files in os.walk(path):
      for d in dirs:
        full_path = os.path.join(root, d)
        if full_path.endswith('.framework') or full_path.endswith('.app') or full_path.endswith('.appex'):
          if not check_universal_build_bundle_darwin(full_path):
            error_msg += 'bundle "%s" is not universal built\n' % full_path
            status_msg += 'bundle "%s" is not universal built\n' % full_path
            checked = False
          else:
            status_msg += 'bundle "%s" is universal built\n' % full_path

      for f in files:
        full_path = os.path.join(root, f)
        if full_path.endswith('.dylib') or full_path.endswith('.so'):
          if not check_universal_build_dylib_darwin(full_path):
            error_msg += 'dylib "%s" is not universal built\n' % full_path
            status_msg += 'dylib "%s" is not universal built\n' % full_path
            checked = False
          else:
            status_msg += 'dylib "%s" is universal built\n' % full_path

  if path.endswith('.dylib') or path.endswith('.so'):
    if not check_universal_build_dylib_darwin(path):
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
    if sys.platform == 'darwin':
        _check_universal_build_darwin(get_app_name(), verbose = True)


def postbuild_archive():
  if sys.platform == 'win32':
    src = get_app_name()
    dst = '%s-windows-%s' % (DEFAULT_ARCH, get_app_name())
  else:
    src = get_app_name()
    dst = '%s-%s' % (sys.platform, get_app_name())
  os.rename(src, dst)

  from zipfile import ZipFile, ZIP_DEFLATED, ZipInfo
  with ZipFile(dst + '.zip', 'w', compression=ZIP_DEFLATED) as archive:
    archive.write(dst, dst, ZIP_DEFLATED)
    if os.path.exists(APP_NAME + '.pdb'):
      archive.write(APP_NAME + '.pdb', APP_NAME + '.pdb', ZIP_DEFLATED)
    for root, dirs, files in os.walk(dst):
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

  output = dst + '.zip'
  archive = os.path.join('..', output)

  try:
    os.unlink(archive)
  except:
    pass
  os.rename(output, archive)

  return [ output ]

if __name__ == '__main__':
  configuration_type = DEFAULT_BUILD_TYPE

  find_source_directory()
  generate_buildscript(configuration_type)
  execute_buildscript(configuration_type)
  if DEFAULT_CLANG_TIDY:
    print('done')
    sys.exit(0)
  postbuild_copy_libraries()
  postbuild_fix_rpath()
  if sys.platform == 'darwin':
    postbuild_fix_retina_display()
    postbuild_codesign()
  if DEFAULT_ENABLE_OSX_UNIVERSAL_BUILD:
    postbuild_check_universal_build()
  outputs = postbuild_archive()
  for output in outputs:
    print('------ %s' % output)
  print('done')
