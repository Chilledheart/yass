// +build !freebsd

package main

func getFreebsdABI(defaultABI int) int {
	return defaultABI;
}
