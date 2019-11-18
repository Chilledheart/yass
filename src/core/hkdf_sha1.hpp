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
 */

#define SUBKEY_INFO "ss-subkey"

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
