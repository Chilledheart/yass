//go:build freebsd
// +build freebsd

package main

import (
	"golang.org/x/sys/unix"
	"strconv"
	"strings"
)

func getFreebsdABI(defaultABI int) int {
	var utsname unix.Utsname
	err := unix.Uname(&utsname)
	if err != nil {
		return defaultABI
	}
	// kern.osrelease: 11.2-RELEASE-p4
	osrelease := string(utsname.Release[:])
	releases := strings.SplitN(osrelease, "-", 3)
	vers := strings.SplitN(releases[0], ".", 2)
	var ver int
	ver, err = strconv.Atoi(vers[0])
	if err != nil {
		return defaultABI
	}
	return ver
}
