# Yet Another Shadow Socket

## Windows 7
<img width="484" alt="snapshot-win7" src="https://user-images.githubusercontent.com/54673341/69158208-02fa3600-0b21-11ea-9bf4-c38e26ff38a3.png">

## Once upon a time, there are (virtual) Nics, so are (shadow) Sockets.

YASS(Yet-Another-Shadow-Socket) acts as a modern cplusplus port of the existing [shadowsocks](http://github.com/shadowsocks) project.

For now, it is heavily developed under macOS.

## History
Originally, ss is written to deal with TCP Relay Proxy and provide the similar functionality with openssh's builtin SOCKS proxy (provided with '-D' option).

It is not so good to stay as "SOCKS" (meaning silly rocket some how), but at most times programs are silly designed. Another port would make it stronger, so here we come with "Yet-Another-Shadow-Socket".

## Benefit
- Better scale, less memory consumption and CPU cycles, less than 0.1% on my iMac.
- Easier to maintain and adopt new features, so far [aead][aead] ciphers supported.
- Safe memory layout, tested against [Address Sanitizer][asan].

## License
It is dual-licensed with GPLv2 and Boost Software License.

## Crypto
### Ciphers for fips modules
- [AEAD_AES_128_GCM][aes128gcm]
- [AEAD_AES_256_GCM][aes256gcm]
- [AEAD_AES_128_GCM_12][aes128gcm12]
- [AEAD_AES_192_GCM][aes192gcm]

### Ciphers for [tls 1.3][tls13]
- [AEAD_CHACHA20_POLY1305][chacha20]
- [AEAD_XCHACHA20_POLY1305][chacha20]

### Operating System
- macOS
- Linux
- Windows

### Protocols supported
- Socks4
- Socks4A
- Socks5
- HTTP/HTTPS

[aead]: https://shadowsocks.org/en/spec/AEAD-Ciphers.html
[asan]: https://github.com/google/sanitizers/wiki/AddressSanitizer
[vcredist]: https://support.microsoft.com/zh-tw/help/2977003/the-latest-supported-visual-c-downloads
[aes128gcm]: https://tools.ietf.org/html/rfc5116
[aes128gcm12]: https://tools.ietf.org/html/rfc5282
[aes192gcm]: https://tools.ietf.org/html/rfc55282
[aes256gcm]: https://tools.ietf.org/html/rfc5116
[chacha20]: https://tools.ietf.org/html/rfc7539
[tls13]: https://tools.ietf.org/html/rfc7905
