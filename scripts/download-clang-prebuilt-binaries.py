#!/usr/bin/env python3
import os
import subprocess
import platform
import errno
import sys
import shutil

# mkdir -p third_party/llvm-build/Release+Asserts
# CLANG_REVISION=$(< CLANG_REVISION)
# curl https://commondatastorage.googleapis.com/chromium-browser-clang/$CLANG_ARCH/clang-$CLANG_REVISION.tgz | tar xzf - -C third_party/llvm-build/Release+Asserts
# curl https://commondatastorage.googleapis.com/chromium-browser-clang/$CLANG_ARCH/clang-tidy-$CLANG_REVISION.tgz | tar xzf - -C third_party/llvm-build/Release+Asserts

def mkdir_p(path):
  try:
    os.makedirs(path)
  except OSError as e: # Python >= 2.5
    if e.errno == errno.EEXIST and os.path.isdir(path):
      pass
    # possibly handle other errno cases here, otherwise finally:
    else:
      raise

def write_output(command, suppress_error=False):
  print('--- %s' % ' '.join(command))
  proc = subprocess.Popen(command, stdout=sys.stdout, stderr=sys.stdout,
      shell=False, env=os.environ)
  proc.communicate()
  if not suppress_error and proc.returncode != 0:
    raise RuntimeError('cmd "%s" failed, exit code %d' % (' '.join(command),
                                                          proc.returncode))
def main():
  os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

  with open('CLANG_REVISION', 'r') as f:
    clang_revision = f.read().strip()

  if platform.system() == 'Linux' and sys.maxsize > 2**32:
    clang_arch = 'Linux_x64'
    exe_suffix = ''
  elif platform.system() == 'Darwin':
    clang_arch = 'Mac'
    if platform.machine() == 'arm64':
      clang_arch = 'Mac_arm64'
    exe_suffix = ''
  elif platform.system() == 'Windows':
    clang_arch = 'Win'
    exe_suffix = '.exe'
  else:
    raise RuntimeError('no prebuilt binaries exist for this platform: %s' % platform.system())

  if os.path.exists(f'third_party/llvm-build/Release+Asserts/bin/clang{exe_suffix}') and \
    os.path.exists(f'third_party/llvm-build/Release+Asserts/bin/clang-tidy{exe_suffix}'):
    return

  mkdir_p('third_party/llvm-build/Release+Asserts')
  os.chdir('third_party/llvm-build/Release+Asserts')

  write_output(['curl', '-C', '-', '-o', f'clang-{clang_revision}-{clang_arch}.tgz',
                f'https://commondatastorage.googleapis.com/chromium-browser-clang/{clang_arch}/clang-{clang_revision}.tgz'])
  write_output(['curl', '-C', '-', '-o', f'clang-tidy-{clang_revision}-{clang_arch}.tgz',
                f'https://commondatastorage.googleapis.com/chromium-browser-clang/{clang_arch}/clang-tidy-{clang_revision}.tgz'])
  write_output(['tar', '-xzf', f'clang-{clang_revision}-{clang_arch}.tgz'])
  write_output(['tar', '-xzf', f'clang-tidy-{clang_revision}-{clang_arch}.tgz'])

  # create a shim to lld-link
  os.chdir('bin')
  if platform.system() == 'Windows':
    write_output(['clang-cl.exe', '..\..\..\..\scripts\llvm-lib.c', '/DWIN32',
                  '/DWIN32_LEAN_AND_MEAN', '/D_UNICODE', '/DUNICODE', '/MT',
                  '/O2', '/Ob2', '/DNDEBUG', 'shell32.lib'])
  else:
    shutil.copyfile('../../../../scripts/llvm-lib', 'llvm-lib')
    shutil.copymode('../../../../scripts/llvm-lib', 'llvm-lib')
    # still missing llvm-rc

if __name__ == '__main__':
  main()
