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
    print(f'{pkg_url} checksum mismatched, expected: {pkg_sum}!')
    return
  write_output(['tar', '-C', dst, '-xvf', os.path.basename(pkg_url), '/usr/local/include', '/usr/local/libdata', '/usr/local/lib'])

def main(args):
  if not args:
    print("no abi specified, setting to freebsd11")
    abi = '11'
  else:
    abi = args[0]

  # not all tarbars exist in public sever
  if abi == '11':
    release = '3'
  elif abi == '12':
    release = '3'
  elif abi == '13':
    release = '0'
  else:
    release = '0'
  version = f'{abi}.{release}'

  sysroot = f'freebsd-{abi}-toolchain'

  print('extracting sysroot...')

  if not os.path.isdir(sysroot):
    os.mkdir(sysroot)

  # extract include and so only
  write_output(['curl', '-C', '-', '-L', '-O', f'http://ftp.freebsd.org/pub/FreeBSD/releases/amd64/{version}-RELEASE/base.txz'])
  write_output(['tar', '-C', sysroot, '-xvf', 'base.txz', './usr/include', './usr/lib', './lib', './usr/libdata/pkgconfig'])

  print(f'extract sysroot (gtk3)...')

  base_url = f'http://pkg.freebsd.org/FreeBSD%3A{abi}%3Aamd64/release_{release}'
  write_output(['curl', '-C', '-', '-L', '-O', f'{base_url}/packagesite.txz'])

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
    extract_pkg(base_url + '/' + pkg['path'], pkg['sum'], sysroot)

  print(f'clean up sysroot...')
  # remove remaining files in lib
  import glob
  cmd = ['rm', '-rf']
  cmd.append(f'{sysroot}/usr/lib/clang')
  for f in glob.glob(f'{sysroot}/usr/lib/*.a'):
    if f.endswith('libc_nonshared.a'):
      continue
    cmd.append(f)
  for f in glob.glob(f'{sysroot}/usr/local/lib/*.a'):
    cmd.append(f)
  for f in glob.glob(f'{sysroot}/usr/local/lib/*.a'):
    cmd.append(f)
  for f in glob.glob(f'{sysroot}/usr/local/lib/python*'):
    cmd.append(f)
  write_output(cmd)

  return 0

if __name__ == '__main__':
  main(sys.argv[1:])
