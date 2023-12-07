#!/usr/bin/env python3
"""Install android ndk for building yass.
"""
import hashlib
import os
import shutil
import sys
import zipfile

# checked with https://source.chromium.org/chromium/chromium/src/+/main:build/config/android/config.gni?q=default_android_ndk_version&ss=chromium
DEFAULT_ANDROID_NDK_VERSION = 'r25c'

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

# cp -r --parents sources/android/cpufeatures ../third_party/android_toolchain
# cp -r --parents toolchains/llvm/prebuilt ../third_party/android_toolchain
# find toolchains -type f -regextype egrep \! -regex \
#  '.*(lib(atomic|gcc|gcc_real|compiler_rt-extras|android_support|unwind).a|crt.*o|lib(android|c|dl|log|m).so|usr/local.*|usr/include.*)' -delete
def extract_zipfile(tar, sysroot=".", filters=[], dir_filters=[]):
  print('extracting %s with (filters %s)' % (tar, ' '.join(filters)))
  safe_archive_names = [ 'libatomic.a', 'libgcc.a', 'libgcc_real.a',
                         'libcompiler_rt-extras.a', 'libandroid_support.a', 'libunwind.a',
                         'libandroid.so', 'libc.so', 'libdl.so', 'liblog.so', 'libm.so',
                         'libc++.so', 'libc++.a', 'libc++_shared.so',
                         'libc++_static.a', 'libstdc++.so', 'libcamera2ndk.so',
                         'libEGL.so', 'libGLESv1_CM.so',
                         'libGLESv2.so', 'libGLESv3.so',
                         'libjnigraphics.so', 'libvulkan.so', 'libz.so',
                       ]
  with zipfile.ZipFile(tar, 'r') as package_tar:
    members = package_tar.namelist()
    filtered_members = []
    for member in members:
      filtered = False if filters else True
      dir_filtered = False
      basename = os.path.basename(member)
      dirname = os.path.dirname(member)
      for filter in filters:
        _dirname = dirname + "/"
        if _dirname.startswith(filter + "/"):
          filtered = True
          break;
      for dir_filter in dir_filters:
        if dirname.startswith(dir_filter):
          filtered = True
          dir_filtered = True
          break;
      # remove all '.a' archive files except for libc_nonshared.a and libssp_nonshared.a
      # remove all python-related files inside /usr/local/lib
      if filtered and not dir_filtered:
        filtered = False
        if 'include' in dirname:
          filtered = True
        if basename.endswith('.o'):
          filtered = True
        if basename.endswith('-android.a') or basename.endswith('-android.so'):
          filtered = True
        if basename in safe_archive_names:
          filtered = True
      if filtered:
        filtered_members.append(member)
    if not filtered_members:
      return
    package_tar.extractall(sysroot, filtered_members)

def usage():
  print("usage: ./install-sysroot-android.py <ndk_version>")
  sys.exit(-1)

def main(args):
  ndk_version = DEFAULT_ANDROID_NDK_VERSION
  if not args:
    print("no abi specified, setting to android " + DEFAULT_ANDROID_NDK_VERSION)
  elif args and len(args) == 1:
    ndk_version = args[0]
  else:
    usage()


  ndk_url = f'https://dl.google.com/android/repository/android-ndk-{ndk_version}-linux.zip'
  ndk_tarball = f'android-ndk-{ndk_version}-linux.zip'
  ndk_prefix = f'android-ndk-{ndk_version}'

  tmproot = os.path.abspath(f'android-{ndk_version}-tmp')
  sysroot = os.path.abspath(f'third_party/android_toolchain')

  if not os.path.isdir(tmproot):
    os.mkdir(tmproot)
  os.chdir(tmproot)

  # remove old sysroot files
  if os.path.isdir(sysroot):
    shutil.rmtree(sysroot)
  os.mkdir(sysroot)

  download_url(ndk_url, ndk_tarball)

  print(f'Extracting sysroot to {tmproot}...')
  extract_zipfile(ndk_tarball, tmproot,
                  [f'{ndk_prefix}/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/14.0.7/lib/linux',
                   f'{ndk_prefix}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib',
                   f'{ndk_prefix}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/local/include',
                   f'{ndk_prefix}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include'],
                  [f'{ndk_prefix}/sources/android/cpufeatures',
                   f'{ndk_prefix}/sources/android/native_app_glue']
                 )
  dir_list = os.listdir(f'{tmproot}/{ndk_prefix}')
  for dir_item in dir_list:
    if os.path.isdir(f'{tmproot}/{ndk_prefix}/{dir_item}'):
      print(f'Moving {dir_item} to {sysroot}...')
      os.rename(f'{tmproot}/{ndk_prefix}/{dir_item}', f'{sysroot}/{dir_item}')

  # remove tmp files
  shutil.rmtree(tmproot)

  return 0

if __name__ == '__main__':
  main(sys.argv[1:])
