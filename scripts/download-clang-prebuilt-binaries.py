#!/usr/bin/env python3
import os
import subprocess
import platform
import errno
import sys
import shutil
import tarfile

# mkdir -p third_party/llvm-build/Release+Asserts
# CLANG_REVISION=$(< CLANG_REVISION)
# curl https://commondatastorage.googleapis.com/chromium-browser-clang/$CLANG_ARCH/clang-$CLANG_REVISION.tgz | tar xzf - -C third_party/llvm-build/Release+Asserts
# curl https://commondatastorage.googleapis.com/chromium-browser-clang/$CLANG_ARCH/clang-tidy-$CLANG_REVISION.tgz | tar xzf - -C third_party/llvm-build/Release+Asserts
# curl https://commondatastorage.googleapis.com/chromium-browser-clang/$CLANG_ARCH/libclang-$CLANG_REVISION.tgz | tar xzf - -C third_party/llvm-build/Release+Asserts
# curl https://commondatastorage.googleapis.com/chromium-browser-clang/$CLANG_ARCH/clangd-$CLANG_REVISION.tgz | tar xzf - -C third_party/llvm-build/Release+Asserts

try:
  # For Python 3.0 and later
  from urllib.request import urlretrieve
except ImportError:
  # Fall back to Python 2's urllib2
  from urllib import urlretrieve

def download_url(url, tarball):
  print('Downloading %s to %s' % (url, tarball))
  sys.stdout.flush()
  sys.stderr.flush()
  for _ in range(3):
    try:
      urlretrieve(url, tarball)
      break
    except Exception:  # Ignore exceptions.
      pass
  else:
    raise Exception('Failed to download %s' % url)

def extract_tarfile(tar):
  print('Extracting %s' % tar)
  with tarfile.open(tar) as package_tar:
    package_tar.extractall()

def mkdir_p(path):
  try:
    os.makedirs(path)
  except OSError as e: # Python >= 2.5
    if e.errno == errno.EEXIST and os.path.isdir(path):
      pass
    # possibly handle other errno cases here, otherwise finally:
    else:
      raise

def write_output(command, check=False):
  print('--- %s' % ' '.join(command))
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
  retcode = proc.returncode
  if check and retcode:
    raise subprocess.CalledProcessError(retcode, proc.args,
                                        output=sys.stdout, stderr=sys.stderr)
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

  mkdir_p('third_party/llvm-build/Release+Asserts')
  os.chdir('third_party/llvm-build/Release+Asserts')

  download_url(f'https://commondatastorage.googleapis.com/chromium-browser-clang/{clang_arch}/clang-{clang_revision}.tgz',
               f'clang-{clang_revision}-{clang_arch}.tgz')
  download_url(f'https://commondatastorage.googleapis.com/chromium-browser-clang/{clang_arch}/clang-tidy-{clang_revision}.tgz',
               f'clang-tidy-{clang_revision}-{clang_arch}.tgz')
  download_url(f'https://commondatastorage.googleapis.com/chromium-browser-clang/{clang_arch}/libclang-{clang_revision}.tgz',
               f'libclang-{clang_revision}-{clang_arch}.tgz')
  download_url(f'https://commondatastorage.googleapis.com/chromium-browser-clang/{clang_arch}/clangd-{clang_revision}.tgz',
               f'clangd-{clang_revision}-{clang_arch}.tgz')
  extract_tarfile(f'clang-{clang_revision}-{clang_arch}.tgz')
  extract_tarfile(f'clang-tidy-{clang_revision}-{clang_arch}.tgz')
  extract_tarfile(f'libclang-{clang_revision}-{clang_arch}.tgz')
  extract_tarfile(f'clangd-{clang_revision}-{clang_arch}.tgz')

  # create a shim to lld-link
  os.chdir('bin')
  if platform.system() == 'Windows':
    write_output(['clang-cl.exe', '..\..\..\..\scripts\llvm-lib.c', '/DWIN32',
                  '/DWIN32_LEAN_AND_MEAN', '/D_UNICODE', '/DUNICODE', '/MT',
                  '/O2', '/Ob2', '/DNDEBUG', 'shell32.lib'], check=True)
  else:
    write_output(['ln', '-sf', 'llvm-ar', 'llvm-lib'], check=True)
    write_output(['ln', '-sf', 'llvm-ar', 'llvm-ranlib'], check=True)
    # still missing llvm-rc

if __name__ == '__main__':
  main()
