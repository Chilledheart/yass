#!/usr/bin/env python3

import subprocess

def check_string_output(command):
  return subprocess.check_output(command, stderr=subprocess.STDOUT).decode().strip()

# https://wiki.debian.org/DebianReleases
# https://wiki.ubuntu.com/Releases
codenames = {
  "trusty" : "ubuntu-14.04-trusty",
  "xenial" : "ubuntu-16.04-xenial",
  "bionic" : "ubuntu-18.04-bionic",
  "focal" : "ubuntu-20.04-focal",
  "impish" : "ubuntu-21.10-impish",
  "jammy" : "ubuntu-22.04-jammy",
  "lunar" : "ubuntu-23.04-lunar",
  "mantic" : "ubuntu-23.10-mantic",
  "stretch" : "debian-9-stretch",
  "buster" : "debian-10-buster",
  "bullseye" : "debian-11-bullseye",
  "bookworm" : "debian-12-bookworm",
  "trixie" : "debian-13-trixie",
}

def main():
  import argparse
  parser = argparse.ArgumentParser()
  parser.add_argument('codename', nargs='?', help='Debian Distribution Name')
  args = parser.parse_args()

  codename = args.codename
  if not codename:
    codename = check_string_output(['lsb_release', '-sc']).lower()

  if codename in codenames:
    print(codenames[codename])
  else:
    id = check_string_output(['lsb_release', '-si']).lower()
    release = check_string_output(['lsb_release', '-sr'])
    print("%s-%s-%s" % (id, release, codename))

if __name__ == '__main__':
  main()
