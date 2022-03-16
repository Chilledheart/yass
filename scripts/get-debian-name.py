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
  parser.add_argument('--id', help='Debian Distribution ID')
  args = parser.parse_args()

  codename = args.codename
  if not codename:
    codename = check_string_output(['lsb_release', '-sc'])

  if codename in codenames:
    print(codenames[codename])
  else:
    id = args.id
    if not id:
      id = check_string_output(['lsb_release', '-si'])
    print("%s-unknown-%s" % (id, codename))

if __name__ == '__main__':
  main()
