//
// hkdf_sha1.hpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef H_HKDF_SHA1
#define H_HKDF_SHA1

/*
 * Spec: http://shadowsocks.org/en/spec/AEAD-Ciphers.html
 *
 * The way Shadowsocks using AEAD ciphers is specified in SIP004 and amended in
 * SIP007. SIP004 was proposed by @Mygod with design inspirations from
 * @wongsyrone, @Noisyfox and @breakwa11. SIP007 was proposed by @riobard with
 * input from
 * @madeye, @Mygod, @wongsyrone, and many others.
 *
 * Key Derivation
 *
 * HKDF_SHA1 is a function that takes a secret key, a non-secret salt, an info
 * string, and produces a subkey that is cryptographically strong even if the
 * input secret key is weak.
 *
 *      HKDF_SHA1(key, salt, info) => subkey
 *
 * The info string binds the generated subkey to a specific application context.
 * In our case, it must be the string "ss-subkey" without quotes.
 *
 * We derive a per-session subkey from a pre-shared master key using HKDF_SHA1.
 * Salt must be unique through the entire life of the pre-shared master key.
 *
 * TCP
 *
 * An AEAD encrypted TCP stream starts with a randomly generated salt to derive
 * the per-session subkey, followed by any number of encrypted chunks. Each
 * chunk has the following structure:
 *
 *      [encrypted payload length][length tag][encrypted payload][payload tag]
 *
 * Payload length is a 2-byte big-endian unsigned integer capped at 0x3FFF. The
 * higher two bits are reserved and must be set to zero. Payload is therefore
 * limited to 16*1024 - 1 bytes.
 *
 * The first AEAD encrypt/decrypt operation uses a counting nonce starting from
 * 0. After each encrypt/decrypt operation, the nonce is incremented by one as
 * if it were an unsigned little-endian integer. Note that each TCP chunk
 * involves two AEAD encrypt/decrypt operation: one for the payload length, and
 * one for the payload. Therefore each chunk increases the nonce twice.
 *
 * UDP
 *
 * An AEAD encrypted UDP packet has the following structure:
 *
 *      [salt][encrypted payload][tag]
 *
 * The salt is used to derive the per-session subkey and must be generated
 * randomly to ensure uniqueness. Each UDP packet is encrypted/decrypted
 * independently, using the derived subkey and a nonce with all zero bytes.
 *
 */

int crypto_hkdf_extract(const unsigned char *salt, int salt_len,
                        const unsigned char *ikm, int ikm_len,
                        unsigned char *prk);

int crypto_hkdf_expand(const unsigned char *prk, int prk_len,
                       const unsigned char *info, int info_len,
                       unsigned char *okm, int okm_len);

/* HKDF-Extract + HKDF-Expand */
int crypto_hkdf(const unsigned char *salt, int salt_len,
                const unsigned char *ikm, int ikm_len,
                const unsigned char *info, int info_len,
                unsigned char *okm, int okm_len);

#endif // H_HKDF_SHA1
