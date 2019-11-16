//
// config_impl.hpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CONFIG_CONFIG_IMPL
#define H_CONFIG_CONFIG_IMPL

#include <memory>
#include <string>

namespace config {
class ConfigImpl {
public:
  virtual ~ConfigImpl();

  static std::unique_ptr<ConfigImpl> Create();

  virtual bool Open(bool dontread) = 0;
  virtual bool Close() = 0;

  virtual bool Read(const std::string &key, std::string *value) = 0;
  virtual bool Read(const std::string &key, uint32_t *value) = 0;
  virtual bool Read(const std::string &key, int32_t *value) = 0;
  virtual bool Read(const std::string &key, uint64_t *value) = 0;
  virtual bool Read(const std::string &key, int64_t *value) = 0;

  virtual bool Write(const std::string &key, const std::string &value) = 0;
  virtual bool Write(const std::string &key, uint32_t value) = 0;
  virtual bool Write(const std::string &key, int32_t value) = 0;
  virtual bool Write(const std::string &key, uint64_t value) = 0;
  virtual bool Write(const std::string &key, int64_t value) = 0;
};
} // namespace config

#endif // H_CONFIG_CONFIG_IMPL
