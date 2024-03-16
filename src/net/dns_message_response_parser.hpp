// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_NET_DNS_MESSAGE_RESPONSE_PARSER
#define H_NET_DNS_MESSAGE_RESPONSE_PARSER

#include <cstdlib>

#include "core/logging.hpp"
#include "net/dns_message_response.hpp"

namespace net {

namespace dns_message {
class response;
class response_parser {
 public:
  /// Construct ready to parse the response method.
  response_parser();

  /// Reset to initial parser state.
  void reset();

  /// Result of parse.
  enum result_type { good, bad, indeterminate };

  /// Parse some data. The enum return value is good when a complete response has
  /// been parsed, bad if the data is invalid, indeterminate when more data is
  /// respuired. The InputIterator return value indicates how much of the input
  /// has been consumed.
  template <typename InputIterator>
  std::tuple<result_type, InputIterator> parse(response& resp,
                                               InputIterator init,
                                               InputIterator begin,
                                               InputIterator end) {
    InputIterator i = begin;
    switch (state_) {
      case response_start:
        if (end - i < (int)sizeof(header)) {
          return std::make_tuple(indeterminate, i);
        }
        memcpy(&resp.header_, &*i, sizeof(header));
        if (resp.header_.qr != 1) {
          return std::make_tuple(bad, i);
        }
        VLOG(3) << "dns_message: response:" << std::hex << " id: 0x" << (int)resp.id() << std::dec
                << " qr: " << (int)resp.header_.qr << " opcode: " << (int)resp.header_.opcode
                << " aa: " << (int)resp.header_.aa << " tc: " << (int)resp.header_.tc << " rd: " << (int)resp.header_.rd
                << " ra: " << (int)resp.header_.ra << " z: " << (int)resp.header_.z
                << " rcode: " << (int)resp.header_.rcode;

        {
          uint16_t qdcount = get16bit(reinterpret_cast<const uint8_t*>(&resp.header_.qdcount));
          uint16_t ancount = get16bit(reinterpret_cast<const uint8_t*>(&resp.header_.ancount));
          uint16_t nscount = get16bit(reinterpret_cast<const uint8_t*>(&resp.header_.nscount));
          uint16_t arcount = get16bit(reinterpret_cast<const uint8_t*>(&resp.header_.arcount));
          VLOG(3) << "dns_message: response: qdcount: " << (int)qdcount << " ancount: " << (int)ancount
                  << " nscount: " << (int)nscount << " arcount: " << (int)arcount;
        }

        i += sizeof(header);
        state_ = response_qd_start;
        return parse(resp, init, i, end);
      case response_qd_start: {
        uint16_t qdcount = get16bit(reinterpret_cast<const uint8_t*>(&resp.header_.qdcount));
        while (qdcount) {
          if (!skipqname(&i, end)) {
            return std::make_tuple(indeterminate, i);
          }
          if (end - i < 4)
            return /*DOH_DNS_OUT_OF_RANGE*/ std::make_tuple(indeterminate, i);
          i += 4; /* skip question's type and class */
          VLOG(3) << "dns_message: skip one qd field";
          --qdcount;
        }
        state_ = response_an_start;
        return parse(resp, init, i, end);
      }
      case response_an_start: {
        uint16_t ancount = get16bit(reinterpret_cast<const uint8_t*>(&resp.header_.ancount));
        while (ancount) {
          unsigned short clazz;
          unsigned int ttl;
          if (!skipqname(&i, end)) {
            return std::make_tuple(indeterminate, i);
          }

          // get type
          if (end - i < 2)
            return /*DOH_DNS_OUT_OF_RANGE*/ std::make_tuple(indeterminate, i);
          auto type = get16bit(i);
          VLOG(3) << "dns_message: an: type: " << type;
          if (type != DNS_TYPE_A && type != DNS_TYPE_AAAA && type != DNS_TYPE_CNAME && type != DNS_TYPE_DNAME) {
            /* Not the same type as was asked for nor CNAME nor DNAME */
            return /*DOH_DNS_UNEXPECTED_TYPE*/ std::make_tuple(indeterminate, i);
          }
          i += 2;

          if (end - i < 2)
            return /*DOH_DNS_OUT_OF_RANGE*/ std::make_tuple(indeterminate, i);
          clazz = get16bit(i);
          VLOG(3) << "dns_message: an: class: " << clazz;
          if (DNS_CLASS_IN != clazz)
            return /*DOH_DNS_UNEXPECTED_CLASS*/ std::make_tuple(indeterminate, i); /* unsupported */
          i += 2;

          if (end - i < 4)
            return /*DOH_DNS_OUT_OF_RANGE*/ std::make_tuple(indeterminate, i);
          ttl = get32bit(i);
          VLOG(3) << "dns_message: an: ttl: " << ttl;
          i += 4;

          if (end - i < 2)
            return /*DOH_DNS_OUT_OF_RANGE*/ std::make_tuple(indeterminate, i);
          auto rdlength = get16bit(i);
          VLOG(3) << "dns_message: an: rdlength: " << rdlength;
          i += 2;

          if (end - i < rdlength)
            return /*DOH_DNS_OUT_OF_RANGE*/ std::make_tuple(indeterminate, i);
          if (!rdata(resp, init, i, end, rdlength, type)) {
            return std::make_tuple(indeterminate, i);
          }
          i += rdlength;
          --ancount;
          VLOG(3) << "dns_message: add one an field";
        }
        state_ = response_ns_start;
        return parse(resp, init, i, end);
      }
      case response_ns_start: {
        uint16_t nscount = get16bit(reinterpret_cast<const uint8_t*>(&resp.header_.nscount));
        while (nscount) {
          if (!skipqname(&i, end)) {
            return std::make_tuple(indeterminate, i);
          }
          if (end - i < 8)
            return /*DOH_DNS_OUT_OF_RANGE*/ std::make_tuple(indeterminate, i);
          i += 2 + 2 + 4; /* type, class and ttl */

          if (end - i < 2)
            return /*DOH_DNS_OUT_OF_RANGE*/ std::make_tuple(indeterminate, i);
          auto rdlength = get16bit(i);
          i += 2;
          if (end - i < rdlength)
            return /*DOH_DNS_OUT_OF_RANGE*/ std::make_tuple(indeterminate, i);
          i += rdlength;
          --nscount;
          VLOG(3) << "dns_message: skip one ns field";
        }
        state_ = response_ar_start;
        return parse(resp, init, i, end);
      }
      case response_ar_start: {
        uint16_t arcount = get16bit(reinterpret_cast<const uint8_t*>(&resp.header_.arcount));
        while (arcount) {
          if (!skipqname(&i, end)) {
            return std::make_tuple(indeterminate, i);
          }
          if (end - i < 8)
            return /*DOH_DNS_OUT_OF_RANGE*/ std::make_tuple(indeterminate, i);
          i += 2 + 2 + 4; /* type, class and ttl */

          if (end - i < 2)
            return /*DOH_DNS_OUT_OF_RANGE*/ std::make_tuple(indeterminate, i);
          auto rdlength = get16bit(i);
          i += 2;
          if (end - i < rdlength)
            return /*DOH_DNS_OUT_OF_RANGE*/ std::make_tuple(indeterminate, i);
          i += rdlength;
          --arcount;
          VLOG(3) << "dns_message: skip one ar field";
        }
        break;
      }
      default:
        return std::make_tuple(bad, i);
    }
    return std::make_tuple(good, i);
  }

 private:
  template <typename InputIterator>
  static unsigned short get16bit(InputIterator it) {
    auto doh = reinterpret_cast<const uint8_t*>(it);
    return (unsigned short)((doh[0] << 8) | doh[1]);
  }

  template <typename InputIterator>
  static unsigned int get32bit(InputIterator it) {
    /* make clang and gcc optimize this to bswap by incrementing
       the pointer first. */
    auto doh = reinterpret_cast<const uint8_t*>(it);

    /* avoid undefined behavior by casting to unsigned before shifting
       24 bits, possibly into the sign bit. codegen is same, but
       ub sanitizer won't be upset */
    return ((unsigned)doh[0] << 24) | ((unsigned)doh[1] << 16) | ((unsigned)doh[2] << 8) | doh[3];
  }
  template <typename InputIterator>
  static bool skipqname(InputIterator* begin, InputIterator end) {
    unsigned char length;
    unsigned short dohlen = end - *begin;
    do {
      if (dohlen < 1)
        return /*DOH_DNS_OUT_OF_RANGE*/ false;
      length = **begin;
      if ((length & 0xc0) == 0xc0) {
        /* name pointer, advance over it and be done */
        if (dohlen < 2)
          return /*DOH_DNS_OUT_OF_RANGE*/ false;
        *begin += 2;
        break;
      }
      if (length & 0xc0)
        return /*DOH_DNS_BAD_LABEL*/ false;
      if (dohlen < (1 + length))
        return /*DOH_DNS_OUT_OF_RANGE*/ false;
      *begin += (unsigned int)(1 + length);
    } while (length);
    return true;
  }

  template <typename InputIterator>
  bool rdata(response& resp,
             InputIterator init,
             InputIterator begin,
             InputIterator end,
             unsigned short rdlength,
             unsigned short type) {
    switch (type) {
      case DNS_TYPE_A:
        if (rdlength != 4) {
          return /*DOH_DNS_RDATA_LEN*/ false;
        }
        {
          const auto* bytes = reinterpret_cast<const asio::ip::address_v4::bytes_type*>(begin);
          asio::ip::address_v4 address(*bytes);
          resp.a_.push_back(address);
          VLOG(3) << "dns_message: an add ipv4: " << address;
        }
        break;
      case DNS_TYPE_AAAA:
        if (rdlength != 16) {
          return /*DOH_DNS_RDATA_LEN*/ false;
        }
        {
          const auto* bytes = reinterpret_cast<const asio::ip::address_v6::bytes_type*>(begin);
          asio::ip::address_v6 address(*bytes);
          resp.aaaa_.push_back(address);
          VLOG(3) << "dns_message: an add ipv6: " << address;
        }
        break;
      case DNS_TYPE_CNAME:
        resp.cname_.push_back(std::string());
        while (true) {
          unsigned char length = *begin;
          if ((length & 0xc0) == 0xc0) {
            /* name pointer, get the new offset (14 bits) */
            if (end - begin < 1) {
              return /*DOH_DNS_OUT_OF_RANGE*/ false;
            }
            /* move to the new index */
            begin = init + ((length & 0x3f) << 8 | *(begin + 1));
            continue;
          } else if (length & 0xc0) {
            return /*DOH_DNS_BAD_LABEL*/ false;
          } else {
            ++begin;
          }
          if (length) {
            if (end - begin < length) {
              return /*DOH_DNS_BAD_LABEL*/ false;
            }
            std::string* name = &*resp.cname_.rbegin();
            if (!name->empty()) {
              *name += ".";
            }
            *name += std::string(reinterpret_cast<const char*>(&*begin), length);
            begin += length;
          } else {
            break;
          }
        }
        VLOG(3) << "dns_message: an cname: " << *resp.cname_.rbegin();
        break;
      case DNS_TYPE_DNAME:
        /* explicit for clarity; just skip; rely on synthesized CNAME  */
        break;
      default:
        /* unsupported type, just skip it */
        break;
    }
    return true;
  }

  enum state {
    response_start,
    response_qd_start,
    response_an_start,
    response_ns_start,
    response_ar_start,
  } state_;
};

}  // namespace dns_message

}  // namespace net

#endif  // H_NET_DNS_MESSAGE_RESPONSE_PARSER
