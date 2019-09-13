import os
import re
import shutil
import subprocess
import sys
from multiprocessing import cpu_count
from time import sleep

APP_NAME = 'yass'
DEFAULT_BUILD_TYPE = 'MinSizeRel'
DEFAULT_OS_MIN = '10.9'
DEFAULT_WXWIDGETS_FRAMEWORK = '/opt/local/Library/Frameworks/wxWidgets.framework'

num_cpus=cpu_count()

def macdeploywxwidgets(name):
  frameworks_path = os.path.join(name, 'Contents', 'Frameworks', os.path.basename(DEFAULT_WXWIDGETS_FRAMEWORK))
  shutil.copytree(DEFAULT_WXWIDGETS_FRAMEWORK, frameworks_path, symlinks=True)
  lib_path = os.path.join(frameworks_path, 'Versions', 'wxWidgets', '3.1', 'lib')
  libs = os.listdir(lib_path)
  for lib_name in libs:
    lib = os.path.join(lib_path, lib_name)
    if os.path.isdir(lib):
      continue
    write_output(['install_name_tool', '-id', '@loader_path/%s' % os.path.basename(lib), lib])
    write_output(['install_name_tool', '-add_rpath', '@loader_path/../../../../../../Frameworks/', lib])
    write_output(['install_name_tool', '-add_rpath', '@loader_path/', lib])
    write_output(['install_name_tool', '-delete_rpath', '/usr/local/lib', lib])
    write_output(['install_name_tool', '-delete_rpath', '/opt/local/lib', lib])
    deps = get_dependencies(lib)
    for dep in deps:
      dep_name = os.path.basename(dep)
      if '.framework' in dep:
        write_output(['install_name_tool', '-change', dep, '@loader_path/%s' % dep_name, lib])
      else:
        write_output(['install_name_tool', '-change', dep, '@loader_path/../../../../../../Frameworks/%s' % dep_name, lib])

def write_output(command):
  proc = subprocess.Popen(command, stdout=sys.stdout, stderr=sys.stdout, shell=False)
  proc.communicate()

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
  if os.name == 'winnt':
    return get_dependencies_by_objdump(path)
  elif sys.platform == 'darwin':
    return get_dependencies_by_otool(path)
  elif sys.platform == 'linux':
    return get_dependencies_by_ldd(path)
  else:
    raise IOError('not supported in platform %s' % sys.platform)

def get_dependencies_recursively(path):
  if os.name == 'winnt':
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

def postbuild_copy_libraries():
  print 'copying dependent libraries...'
  if sys.platform == 'darwin':
      postbuild_copy_libraries_xcode()
  else:
      postbuild_copy_libraries_posix()

def postbuild_copy_libraries_posix():
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

def postbuild_copy_libraries_xcode():
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
    shutil.copyfile(lib, frameworks_path + '/' + os.path.basename(lib))
  macdeploywxwidgets(APP_NAME + '.app')

def postbuild_fix_rpath():
  print 'fixing rpath...'
  if os.name == 'winnt':
    print 'not need to fix rpath'
  elif sys.platform == 'linux':
    postbuild_patchelf()
  elif sys.platform == 'darwin':
    postbuild_install_name_tool()
  else:
    print 'not supported in platform %s' % sys.platform
  print 'fixing rpath...done'


def postbuild_install_name_tool():
  frameworks_path = os.path.join(APP_NAME + '.app', 'Contents', 'Frameworks')
  resources_path = os.path.join(APP_NAME + '.app', 'Contents', 'Resources')
  macos_path = os.path.join(APP_NAME + '.app', 'Contents', 'MacOS')
  binaries = [os.path.join(macos_path, APP_NAME)]
  for binary in binaries:
    write_output(['install_name_tool', '-add_rpath', '@executable_path/../Frameworks', binary])
    write_output(['install_name_tool', '-add_rpath', '@executable_path/../Frameworks/wxWidgets.framework/Versions/wxWidgets/3.1/lib/', binary])
    deps = get_dependencies(binary)
    for dep in deps:
      dep_name = os.path.basename(dep)
      if '.framework' in dep:
        dep_name = dep[len('/opt/local/Library/Frameworks/'):]
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
    write_output(['install_name_tool', '-add_rpath', '@loader_path/../Frameworks/wxWidgets.framework/Versions/wxWidgets/3.1/lib/', lib])
    write_output(['install_name_tool', '-delete_rpath', '/usr/local/lib', lib])
    write_output(['install_name_tool', '-delete_rpath', '/opt/local/lib', lib])
    deps = get_dependencies(lib)
    for dep in deps:
      dep_name = os.path.basename(dep)
      if '.framework' in dep:
        dep_name = dep[len('/opt/local/Library/Frameworks/'):]
      write_output(['install_name_tool', '-change', dep, '@loader_path/../Frameworks/%s' % dep_name, lib])


def postbuild_patchelf():
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


def postbuild_fix_retina_display():
  print 'fixing retina display...'
  write_output(['plutil', '-insert', 'NSPrincipalClass', '-string', 'NSApplication', APP_NAME + '.app/Contents/Info.plist'])
  write_output(['plutil', '-insert', 'NSHighResolutionCapable', '-bool', 'YES', APP_NAME + '.app/Contents/Info.plist'])
  print 'fixing retina display...done'


def execute_buildscript(generator = 'ninja', configuration = 'Release'):
  print 'executing build scripts...'
  if generator == 'xcode':
    command = ['xcodebuild', '-target', 'ALL_BUILD', '-configuration', configuration, '-jobs', num_cpus]
  elif generator == 'ninja':
    command = ['ninja']
  else:
    command = ['make', '-j', num_cpus]
  write_output(command)
  if generator == 'xcode':
    shutil.copytree(os.path.join(configuration, APP_NAME + '.app'), APP_NAME + '.app')


def generate_buildscript(generator = 'ninja', build_type = DEFAULT_BUILD_TYPE, os_min = DEFAULT_OS_MIN):
  print 'generate build scripts...'
  cmake_args = ['-DCMAKE_BUILD_TYPE=' + build_type]
  cmake_args.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=' + os_min);
  if generator == 'xcode':
    cmake_args.extend(['-G', 'XCode'])
  elif generator == 'ninja':
    cmake_args.extend(['-G', 'Ninja'])
  else:
    cmake_args.extend(['-G', 'Unix Makefiles'])
  command = ['cmake', '..'] + cmake_args
  write_output(command)


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


if __name__ == '__main__':
  find_source_directory()
  generate_buildscript()
  execute_buildscript()
  postbuild_copy_libraries()
  postbuild_fix_rpath()
  postbuild_fix_retina_display()
