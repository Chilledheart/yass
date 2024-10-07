yass_cli(1) -- a lightweight and efficient, socks5/http forward proxy
==========================

## SYNOPSIS

`yass_cli` [_option_...] <br>
`yass_cli` `-c` <file> [_option_...]

## DESCRIPTION

**yass** is initiated as C++ rewrite of the outdated shadowsocks-libev package and
provide the similar functionalities. During the recent development, it also
supports naiveproxy protocol which is more efficient protocol. Compared with
shadowsocks-libev, it not only contains the client cli command and server cli
command, but also it contains a gtk3/gtk4/qt5/qt6 (all are supported) graphical
interface client which is more friendly to the new users.

### How to use
See <https://github.com/Chilledheart/yass/wiki/Usage>.

## OPTIONS

* `-c` _file_, `--configfile` _file_:
  Use specified _file_ as config file.

* `-t`:
  Don't run, just test the configuration file.

* `-v`, `--version`:
  Print yass_cli version.

* `--limit_rate` _rate_:
  Limits the _rate_ of response transmission to a client. Uint can be `(none)`, `k` and `m`.

* `--ipv6_mode`:
  Enable IPv6 support.

* `--server_host` _host_:
  Set the server's hostname or IP.

* `--server_port` _port_:
  Set the server's port number.

* `--local_host` _host_:
  Set the local hostname or IP.

* `--local_port` _port_:
  Set the local port number.

* `--username` _username_:
  Set the _username_. The server and the client should use the same username.

* `--password` _password_:
  Set the _password_. The server and the client should use the same password.

* `--method` _method_:
  Method of encrypt _method_ as required field.
  Allow cipher method depends on your build flags.
  Possible values are:

  _libsodium_ compatible AEAD Cipher:
  `aes-256-gcm`, `chacha20-ietf-poly1305`, `xchacha20-ietf-poly1305`

  _boringssl_ compatible AEAD Cipher:
  `chacha20-ietf-poly1305-evp`, `xchacha20-ietf-poly1305-evp`, `aes-128-gcm-evp`, `aes-128-gcm12-evp`, `aes-192-gcm-evp`, `aes-256-gcm-evp`

  _mbedtls_ compatible STREAM Cipher:
  `aes-128-cfb`, `aes-192-cfb`, `aes-256-cfb`, `aes-128-ctr`, `aes-192-ctr`, `aes-256-ctr`, `camellia-128-cfb`, `camellia-192-cfb`, `camellia-256-cfb`

  _naiveproxy_ compatible Cipher Method:
  `https`, `http2-plaintext`, `http2`

  The default cipher method is `http2`.


* `--connect_timeout` _number_:
  Connect timeout is _number_ in seconds.

* `--worker_connections` _number_:
  Maximum number of accepted connection is _number_.

* `--padding_support`:
  Enable padding support.

* `--use_ca_bundle_crt`:
  Use builtin ca-bundle.crt instead of system CA store.

* `--cacert` _file_:
  Tells where to use the specified certificate _file_ to verify the peer.

* `--capath` _dir_:
  Tells where to use the specified certificate _dir_ to verify the peer.

* `--certificate_chain_file` _file_:
  Use custom certificate chain provided by _file_ to verify server's certificate (optional by https and http2).

* `-k`, `--insecure_mode`:
  This option makes to skip the verification step and proceed without checking.

* `--redir_mode`:
  Enable TCP Redir mode support (Linux only).

* `--tls13_early_data`:
  Enable 0RTTI Early Data.

* `--enable_post_quantum_kyber`:
  Enables post-quantum key-agreements in TLS 1.3 connections. The _use_ml_kem_ flag controls whether ML-KEM or Kyber is used.

* `--use_ml_kem`:
  Use ML-KEM in TLS 1.3. Causes TLS 1.3 connections to use the ML-KEM standard instead of the Kyber draft standard for post-quantum key-agreement. The _enable_post_quantum_kyber_ flag must be enabled for this to have an effect.

* `--congestion_algorithm` _algo_:
  Specify _algo_ as TCP congestion control algorithm for underlying TCP connections (Linux Only)

  See `/proc/sys/net/ipv4/tcp_allowed_congestion_control` for available options and `tcp`(7).

## ENVIRONMENT VARIABLES

* `YASS_CA_BUNDLE`:
  Use as the path of ca-bundle.crt file. Same effect with `--cacert` _file_.

## COPYRIGHT

Copyright (C) 2019-2024 Chilledheart. All rights reserved.

## SEE ALSO

`iptables`(8), `tcp`(7)
