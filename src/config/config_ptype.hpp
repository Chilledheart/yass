// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_CONFIG_CONFIG_PTYPE
#define H_CONFIG_CONFIG_PTYPE

namespace config {

enum ProgramType {
  YASS_UNSPEC,
  YASS_SERVER_MASK = 0x10,
  YASS_SERVER_DEFAULT = 0x10,
  YASS_UNITTEST_MASK = 0x20,
  YASS_UNITTEST_DEFAULT = 0x20,
  YASS_BENCHMARK_MASK = 0x40,
  YASS_BENCHMARK_DEFAULT = 0x40,
  YASS_CLIENT_MASK = 0x80,
  YASS_CLIENT_DEFAULT = 0x80,
  YASS_CLIENT_GUI = 0x81,
};

extern const ProgramType pType;
const char* ProgramTypeToStr(ProgramType type);

inline bool pType_IsClient() {
  return pType & YASS_CLIENT_MASK;
}

inline bool pType_IsServer() {
  return pType & YASS_SERVER_MASK;
}

}  // namespace config

#endif  // H_CONFIG_CONFIG_PTYPE
