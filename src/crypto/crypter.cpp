// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "crypto/crypter.hpp"

namespace crypto {

Crypter::~Crypter() = default;
;

void PacketNumberToNonce(uint8_t* nonce, uint64_t packet_number) {
  uint8_t pn_1 = packet_number & 0xff;
  uint8_t pn_2 = (packet_number & 0xff00) >> 8;
  uint8_t pn_3 = (packet_number & 0xff0000) >> 16;
  uint8_t pn_4 = (packet_number & 0xff000000) >> 24;
  *nonce++ = pn_1;
  *nonce++ = pn_2;
  *nonce++ = pn_3;
  *nonce = pn_4;
}

}  // namespace crypto
