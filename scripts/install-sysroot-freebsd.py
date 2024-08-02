#!/usr/bin/env python3
"""Install FreeBSD sysroots for building yass.
"""
import hashlib
import os
import shutil
import sys
import tarfile
import subprocess

FREEBSD_MAIN_SITE = 'http://ftp.freebsd.org/pub/FreeBSD/releases'
FREEBSD_PKG_SITE = 'http://pkg.freebsd.org'

try:
  # For Python 3.0 and later
  from urllib.request import urlretrieve
except ImportError:
  # Fall back to Python 2's urllib2
  from urllib import urlretrieve

def download_url(url, tarball):
  print(f'downloading {url}...')
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

def check_string_output(command):
  return subprocess.check_output(command, stderr=subprocess.STDOUT).decode().strip()

# ['tar', '-C', sysroot, '-xf', 'base.txz', './usr/include', './usr/lib', './lib', './usr/libdata/pkgconfig']
# ['tar', '-xf', 'packagesite.txz', 'packagesite.yaml']
# ['tar', '-C', sysroot, '-xf', os.path.basename(pkg_url), '/usr/local/include', '/usr/local/libdata', '/usr/local/lib']
def extract_tarfile(tar, sysroot=".", filters=[]):
  print('extracting %s with (filters %s)' % (tar, ' '.join(filters)))
  with tarfile.open(tar) as package_tar:
    members = package_tar.getmembers()
    filtered_members = []
    for member in members:
      filtered = False if filters else True
      path = member.name
      basename = os.path.basename(path)
      dirname = os.path.dirname(path)
      for filter in filters:
        _dirname = dirname + "/"
        if _dirname.startswith(filter + "/"):
          filtered = True
          break;
      # remove all '.a' archive files except for libc_nonshared.a and libssp_nonshared.a
      # remove all python-related files inside /usr/local/lib
      if filtered:
        if basename.endswith('.a'):
          filtered = False
        if basename == 'libc_nonshared.a' or basename == 'libssp_nonshared.a':
          filtered = True
        dirnames = dirname.split('/')
        if dirname.startswith('/usr/local/lib') and len(dirnames) >= 5 and dirnames[4].startswith('python'):
          filtered = False
        if '__pycache__' in dirnames:
          filtered = False
      if filtered:
        if dirname.startswith("/"):
          member.name = "." + member.name
        print(member.name)
        filtered_members.append(member)
    if not filtered_members:
      return
    package_tar.extractall(sysroot, filtered_members)


def GetSha256(filename):
  sha256 = hashlib.sha256()
  with open(filename, 'rb') as f:
    while True:
      # Read in 1mb chunks, so it doesn't all have to be loaded into memory.
      chunk = f.read(1024*1024)
      if not chunk:
        break
      sha256.update(chunk)
  return sha256.hexdigest()

def _resolve_deps(pkg_db, deps):
  resolve_deps = list(deps)
  for dep in deps:
    pkg = pkg_db[dep]
    if 'deps' in pkg:
      resolve_deps.extend([name for name in pkg['deps']])

  return set(resolve_deps)

def resolve_deps(pkg_db, deps):
  while True:
    resolved_deps = _resolve_deps(pkg_db, deps)
    if deps == resolved_deps:
      return deps
    deps = resolved_deps

def extract_pkg(pkg_url, pkg_sum, sysroot, is_zstd):
  pkg_name = os.path.basename(pkg_url)
  download_url(pkg_url, pkg_name)
  if GetSha256(pkg_name) != pkg_sum:
    print(f'{pkg_name} checksum mismatched, expected: {pkg_sum}!')
    sys.exit(-1)
  if is_zstd:
    pkg_tar = pkg_name.replace('.pkg', '.tar')
    print(check_string_output(['zstd', '-d', pkg_name, '-o', pkg_tar, '-f']))
    pkg_name = pkg_tar
  extract_tarfile(pkg_name, sysroot, ['/usr/local/include', '/usr/local/libdata', '/usr/local/lib'])
  if is_zstd:
    os.unlink(pkg_name)

def usage():
  print("usage: ./install-sysroot-freebsd.py <abi> [arch]")
  sys.exit(-1)

def main(args):
  if not args:
    print("no abi specified, setting to freebsd 12 amd64")
    abi = '12'
    arch = 'amd64'
  elif args and len(args) == 2 and str.isdecimal(args[0]) and args[1] in ['amd64', 'i386', 'aarch64']:
    abi = args[0]
    arch = args[1]
  elif args and len(args) == 1 and str.isdecimal(args[0]):
    abi = args[0]
    arch = 'amd64'
  else:
    usage()

  sys_arch = arch if arch != 'aarch64' else 'arm64'

  # not all tarbars exist in public sever
  if abi == '12':
    release = '4'
    is_zstd = False
  elif abi == '13':
    release = '3'
    is_zstd = False
  elif abi == '14':
    release = '1'
    is_zstd = True
  else:
    usage()
  version = f'{abi}.{release}'

  tmproot = os.path.abspath(f'freebsd-{abi}-{arch}-tmp')
  sysroot = os.path.abspath(f'freebsd-{abi}-{arch}-toolchain')

  if not os.path.isdir(tmproot):
    os.mkdir(tmproot)
  os.chdir(tmproot)

  # remove old sysroot files
  if os.path.isdir(sysroot):
    shutil.rmtree(sysroot)
  os.mkdir(sysroot)

  # extract include and shared libraries only
  print('Extracting sysroot (base)...')
  download_url(f'{FREEBSD_MAIN_SITE}/{sys_arch}/{version}-RELEASE/base.txz', 'base.txz')
  extract_tarfile('base.txz', sysroot, ['./usr/include', './usr/lib', './lib', './usr/libdata/pkgconfig'])

  print(f'Extracting sysroot (gtk3)...')
  base_url = f'{FREEBSD_PKG_SITE}/FreeBSD%3A{abi}%3A{arch}/release_{release}'
  download_url(f'{base_url}/packagesite.txz', 'packagesite.txz')
  extract_tarfile('packagesite.txz')

  pkg_db = {}

  with open('packagesite.yaml', 'r', encoding='latin1') as f:
    raw_json = f.read()
    raw_json = raw_json.strip()
    import json
    for raw_pkg in raw_json.split('\n'):
      pkg = json.loads(raw_pkg)
      pkg_db[pkg['name']] = pkg

  deps = resolve_deps(pkg_db, ['gtk3'])
  for dep in deps:
    pkg = pkg_db[dep]
    extract_pkg(base_url + '/' + pkg['path'], pkg['sum'], sysroot, is_zstd)

  # remove tmp files
  shutil.rmtree(tmproot)

  return 0

if __name__ == '__main__':
  main(sys.argv[1:])
