#!/usr/bin/env python
import os
import re
import shutil
import subprocess
import sys
from multiprocessing import cpu_count
from time import sleep

APP_NAME = 'yass'
DEFAULT_BUILD_TYPE = 'Release'
DEFAULT_OS_MIN = '10.10'
DEFAULT_OS_ARCHS = 'arm64;x86_64'
DEFAULT_TOOLSET = 'v142'
DEFAULT_WXWIDGETS_FRAMEWORK = '/opt/local/Library/Frameworks/wxWidgets.framework'

VCPKG_DIR = os.getenv('VCPKG_ROOT')

num_cpus=cpu_count()

def copy_file_with_symlinks(src, dst):
  if os.path.lexists(dst):
    return
  if os.path.islink(src):
    linkto = os.readlink(src)
    os.symlink(linkto, dst)
    return
  shutil.copyfile(src, dst)


def write_output(command):
  proc = subprocess.Popen(command, stdout=sys.stdout, stderr=sys.stdout,
      shell=False, env=os.environ)
  proc.communicate()


def get_app_name():
  if sys.platform == 'win32':
    return '%s.exe' % APP_NAME
  elif sys.platform == 'darwin':
    return '%s.app' % APP_NAME
  return APP_NAME

def macfindframework():
  lib_path = os.path.join(DEFAULT_WXWIDGETS_FRAMEWORK, 'Versions', 'wxWidgets', '3.1', 'lib')
  libs = []
  for lib_name in os.listdir(lib_path):
    lib = os.path.join(lib_path, lib_name)
    if os.path.isdir(lib):
      continue
    is_comp = False
    for comp in components:
      if comp + "-" in lib_name:
        is_comp = True
    if not is_comp:
      continue
    libs.append(lib)

  lib_path = os.path.join(name, 'Contents', 'Frameworks')
  return libs, lib_path

def macdeploywxwidgets(name, components = ['baseu', 'core', 'gl']):
  libs, lib_path = macfindframework()
  for lib in libs:
    dstname = lib_path + '/' + os.path.basename(lib)
    copy_file_with_symlinks(lib, dstname)

    write_output(['install_name_tool', '-id', '@loader_path/%s' % os.path.basename(lib), lib])
    write_output(['install_name_tool', '-add_rpath', '@loader_path/', lib])
    write_output(['install_name_tool', '-delete_rpath', '/usr/local/lib', lib])
    write_output(['install_name_tool', '-delete_rpath', '/opt/local/lib', lib])
    deps = get_dependencies(lib)
    for dep in deps:
      dep_name = os.path.basename(dep)
      write_output(['install_name_tool', '-change', dep, '@loader_path/%s' % dep_name, lib])


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
  print 'todo'
  return []


def get_dependencies_by_dependency_walker(path):
  print 'todo'
  return []


def get_dependencies(path):
  if sys.platform == 'win32':
    return get_dependencies_by_objdump(path)
  elif sys.platform == 'darwin':
    return get_dependencies_by_otool(path)
  elif sys.platform == 'linux':
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
  elif sys.platform == 'linux':
    return get_dependencies_by_ldd(path)
  else:
    raise IOError('not supported in platform %s' % sys.platform)


def _postbuild_copy_libraries_posix():
  lib_path = os.path.join(APP_NAME, 'lib')
  bin_path = os.path.join(APP_NAME, 'bin')
  binaries = [os.path.join(bin_path, APP_NAME)]
  libs = []
  for binrary in binaries:
      libs.extend(get_dependencies_recursively(binrary))
  if not os.path.isdir(lib_path):
      os.makedirs(lib_path)
  for lib in libs:
      shutil.copyfile(lib, lib_path + '/' + os.path.basename(lib))


def _postbuild_copy_libraries_xcode():
  frameworks_path = os.path.join(APP_NAME + '.app', 'Contents', 'Frameworks')
  resources_path = os.path.join(APP_NAME + '.app', 'Contents', 'Resources')
  macos_path = os.path.join(APP_NAME + '.app', 'Contents', 'MacOS')
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
      lib = os.path.dirname(lib) + '/' + os.readlink(lib)
      dstname = frameworks_path + '/' + os.readlink(dstname)
      copy_file_with_symlinks(lib, dstname)
      if os.path.islink(lib):
        lib = os.path.dirname(lib) + '/' + os.readlink(lib)
        dstname = frameworks_path + '/' + os.readlink(dstname)
        copy_file_with_symlinks(lib, dstname)
  #macdeploywxwidgets(APP_NAME + '.app')


def _postbuild_install_name_tool():
  frameworks_path = os.path.join(APP_NAME + '.app', 'Contents', 'Frameworks')
  resources_path = os.path.join(APP_NAME + '.app', 'Contents', 'Resources')
  macos_path = os.path.join(APP_NAME + '.app', 'Contents', 'MacOS')
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
  lib_path = os.path.join(APP_NAME, 'lib')
  bin_path = os.path.join(APP_NAME, 'bin')
  binaries = os.listdir(bin_path)
  for binrary_name in binaries:
    binrary = os.path.join(bin_path, binrary_name)
    if os.path.isdir(binrary):
      continue
    write_output(['patchelf', '-set-rpath', '\\\$ORIGIN/../lib', binrary])
  libs = os.listdir(lib_path)
  for lib_name in libs:
    lib = os.path.join(lib_path, lib_name)
    if os.path.isdir(lib):
      continue
    write_output(['patchelf', '-set-rpath', '\\\$ORIGIN/../lib', lib])


def find_source_directory():
  print 'find source directory...'
  os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
  if not os.path.exists('CMakeLists.txt'):
    print 'Please execute this frome the top dir of the source'
    sys.exit(-1)
  if os.path.exists('build'):
    shutil.rmtree('build')
    sleep(1)
  os.mkdir('build')
  os.chdir('build')


def generate_buildscript(configuration_type):
  print 'generate build scripts...(%s)' % configuration_type
  cmake_args = ['-DGUI=ON', '-DCLI=ON', '-DSERVER=ON']
  if sys.platform == 'win32':
    cmake_args.extend(['-G', 'Visual Studio 16 2019'])
    cmake_args.extend(['-T', DEFAULT_TOOLSET])
    # use Win32 for 32-bit target
    cmake_args.extend(['-A', 'x64'])
    cmake_args.extend(['-DCMAKE_TOOLCHAIN_FILE=%s\\scripts\\buildsystems\\vcpkg.cmake' % VCPKG_DIR])
    cmake_args.extend(['-DCMAKE_GENERATOR_TOOLSET=%s' % DEFAULT_TOOLSET])
    cmake_args.extend(['-DCMAKE_VS_PLATFORM_TOOLSET=%s' % DEFAULT_TOOLSET])
    cmake_args.extend(['-DVCPKG_CRT_LINKAGE=static'])
    cmake_args.extend(['-DVCPKG_LIBRARY_LINKAGE=static'])
    cmake_args.extend(['-DVCPKG_TARGET_TRIPLET=x64-windows-static'])
    cmake_args.extend(['-DVCPKG_ROOT_DIR=%s' % VCPKG_DIR])
    cmake_args.extend(['-DBORINGSSL=ON'])
  else:
    cmake_args.extend(['-G', 'Ninja'])
    cmake_args.extend(['-DCMAKE_BUILD_TYPE=%s' % configuration_type])
    cmake_args.extend(['-DBORINGSSL=ON'])

  if sys.platform == 'darwin':
    cmake_args.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=%s' % DEFAULT_OS_MIN)
    cmake_args.append('-DCMAKE_OSX_ARCHITECTURES=%s' % DEFAULT_OS_ARCHS)

  command = ['cmake', '..'] + cmake_args
  write_output(command)


def execute_buildscript(configuration_type):
  print 'executing build scripts...(%s)' % configuration_type

  command = ['cmake', '--build', '.', '--parallel', '--config', configuration_type, '--target', APP_NAME]
  write_output(command)
  if sys.platform == 'win32':
    src = '%s/%s' % (configuration_type, get_app_name())
    dst = '%s' % get_app_name()
    os.rename(src, dst)

  outputs = ['build/%s' % get_app_name()]

  if sys.platform == 'win32':
    print 'executing build scripts...(debug)'
    command = ['cmake', '--build', '.', '--parallel', '--config', 'Debug']
    write_output(command)

    outputs.append('build\Debug\%s' % get_app_name())

  return outputs


def postbuild_copy_libraries():
  print 'copying dependent libraries...'
  if sys.platform == 'win32':
    # not supported
    pass
  elif sys.platform == 'darwin':
    _postbuild_copy_libraries_xcode()
  else:
    _postbuild_copy_libraries_posix()


def postbuild_fix_rpath():
  print 'fixing rpath...'
  if sys.platform == 'win32':
    # 'no need to fix rpath'
    pass
  elif sys.platform == 'linux':
    _postbuild_patchelf()
  elif sys.platform == 'darwin':
    _postbuild_install_name_tool()
  else:
    print 'not supported in platform %s' % sys.platform


def postbuild_fix_retina_display():
  print 'fixing retina display...'
  write_output(['plutil', '-insert', 'NSPrincipalClass', '-string', 'NSApplication', '%s.app/Contents/Info.plist' % APP_NAME])
  write_output(['plutil', '-insert', 'NSHighResolutionCapable', '-bool', 'YES', '%s.app/Contents/Info.plist' % APP_NAME])

if __name__ == '__main__':
  configuration_type = DEFAULT_BUILD_TYPE

  find_source_directory()
  generate_buildscript(configuration_type)
  outputs = execute_buildscript(configuration_type)
  postbuild_copy_libraries()
  postbuild_fix_rpath()
  if sys.platform == 'darwin':
    postbuild_fix_retina_display()
  for output in outputs:
    print '------ %s' % output
  print 'done'
