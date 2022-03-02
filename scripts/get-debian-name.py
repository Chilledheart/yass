#!/usr/bin/env python3

import subprocess

def check_string_output(command):
  subprocess.check_output(command, stderr=subprocess.STDOUT).decode().strip()

# https://wiki.debian.org/DebianReleases
# https://wiki.ubuntu.com/Releases
codenames = {
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
  parser.add_argument('codename', help='Debian Distribution Name',
                     default=check_string_output(['lsb_release', '-sc']))
  parser.add_argument('--id', help='Debian Distribution ID',
                     default=check_string_output(['lsb_release', '-si']))
  args = parser.parse_args()

  codename = args.codename
  id = args.id

  if codename in codenames:
    print(codenames[codename])
  else:
    print("%s-unknown-%s" % (id, codename))

if __name__ == '__main__':
  main()
