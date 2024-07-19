yass_serevr(1) -- a lightweight and efficient, socks5/http forward proxy
==========================

## SYNOPSIS

`yass_serevr` [_option_...] <br>
`yass_serevr` `-c` <file> [_option_...]

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
  Use custom certificate chain provided by _file_ to verify server's private key (required by https and http2).

* `--private_key_file` _file_:
  Use custom private key provided by _file_ to secure connection between server and client (required by https and http2).

* `--private_key_password` _password_:
  Use custom private key password provided by _password_ to decrypt server's encrypted private key.

* `--user` _user_:
  Set non-privileged user for worker.

* `--group` _group_:
  Set non-privileged group for worker.

* `--hide_ip`:
  If true, the Forwarded header will not be augmented with your IP address.

* `--hide_via`:
  If true, the Via header will not be added.

* `--congestion_algorithm` _algo_:
  Specify _algo_ as TCP congestion control algorithm for underlying TCP connections (Linux Only)

  See `/proc/sys/net/ipv4/tcp_allowed_congestion_control` for available options and `tcp`(7).

## ENVIRONMENT VARIABLES

* `YASS_CA_BUNDLE`:
  Use as the path of ca-bundle.crt file. Same effect with `--cacert` _file_.

## COPYRIGHT

Copyright (C) 2019-2024 Chilledheart. All rights reserved.

## SEE ALSO

`openssl`(1), `tcp`(7)
