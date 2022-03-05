#!/usr/bin/env python3
"""Install FreeBSD sysroots for building yass.
"""
import hashlib
import os
import subprocess
import sys

def check_string_output(command):
  return subprocess.check_output(command, stderr=subprocess.STDOUT).decode().strip()

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

def extract_pkg(pkg_url, pkg_sum, dst):
  print(f'extracting pkg {pkg_url}...')
  write_output(['curl', '-C', '-', '-L', '-O', pkg_url])
  if GetSha256(os.path.basename(pkg_url)) != pkg_sum:
    raise Exception(f'{pkg_url} checksum mismatched!')
  write_output(['tar', '-C', dst, '-xvf', os.path.basename(pkg_url), '/usr'])

def main(args):
  print('extracting sysroot...')
  write_output(['curl', '-C', '-', '-L', '-O', 'https://clickhouse-datasets.s3.yandex.net/toolchains/toolchains/freebsd-11.3-toolchain.tar.xz'])
  write_output(['tar', '-xvf', 'freebsd-11.3-toolchain.tar.xz'])

  sysroot = 'freebsd-11.3-toolchain'

  print('extracting sysroot...(libthr.so and libgcc_s.so)')
  # FIXME extract libthr.so and libgcc_s.so only
  # we might want to repack it ...
  write_output(['curl', '-C', '-', '-L', '-O', 'http://ftp.freebsd.org/pub/FreeBSD/releases/amd64/11.3-RELEASE/base.txz'])
  write_output(['tar', '-C', sysroot, '-xvf', 'base.txz', './lib/libgcc_s.so.1', './lib/libthr.so.3'])
  write_output(['ln', '-sf', 'libthr.so.3', f'{sysroot}/lib/libthr.so'])
  write_output(['ln', '-sf', 'libgcc_s.so.1', f'{sysroot}/lib/libgcc_s.so'])

  print(f'extract sysroot (gtk3)...')
  write_output(['curl', '-C', '-', '-L', '-O', 'http://pkg.freebsd.org/FreeBSD%3A11%3Aamd64/release_3/packagesite.txz'])

  write_output(['tar', '-xvf', 'packagesite.txz', 'packagesite.yaml'])

  pkg_db = {}

  with open('packagesite.yaml', 'r', encoding='latin1') as f:
    raw_json = f.read()
    raw_json = raw_json.strip()
    import json
    for raw_pkg in raw_json.split('\n'):
      pkg = json.loads(raw_pkg)
      pkg_db[pkg['name']] = pkg
  os.unlink('packagesite.yaml')

  deps = resolve_deps(pkg_db, ['gtk3'])
  for dep in deps:
    pkg = pkg_db[dep]
    base_url = 'http://pkg.freebsd.org/FreeBSD%3A11%3Aamd64/release_3'
    extract_pkg(base_url + '/' + pkg['path'], pkg['sum'], sysroot)

  print(f'clean up sysroot...')
  # exclude: include, lib, libdata
  for subdir in ['bin', 'etc', 'lib', 'libexec', 'man', 'openssl', 'sbin', 'share', 'var']:
    if subdir == 'lib':
      import glob
      cmd = ['rm', '-rf']
      for f in glob.glob(f'{sysroot}/usr/local/{subdir}/*.a'):
        cmd.append(f)
      for f in glob.glob(f'{sysroot}/usr/local/{subdir}/python*'):
        cmd.append(f)
      write_output(cmd)
      continue
    write_output(['rm', '-rf', f'{sysroot}/usr/local/{subdir}'])

  return 0

if __name__ == '__main__':
  main(sys.argv[1:])
