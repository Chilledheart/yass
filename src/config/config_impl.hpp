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
  /// Deconstruct the ConfigTree object
  virtual ~ConfigImpl();

  /// Construct the given ConfigTree implementation
  static std::unique_ptr<ConfigImpl> Create();

  /// Open the ConfigTree
  ///
  /// \param dontread don't load from config tree, useful for write-only object
  virtual bool Open(bool dontread) = 0;

  /// Close the ConfigTree, useful to flush the ConfigTree into persistent media
  virtual bool Close() = 0;

  /// Read the key from ConfigTree
  ///
  /// \param key the key value
  /// \param value the output value (string)
  virtual bool Read(const std::string &key, std::string *value) = 0;

  /// Read the key from ConfigTree
  ///
  /// \param key the key value
  /// \param value the output value (uint32_t)
  virtual bool Read(const std::string &key, uint32_t *value) = 0;

  /// Read the key from ConfigTree
  ///
  /// \param key the key value
  /// \param value the output value (int32_t)
  virtual bool Read(const std::string &key, int32_t *value) = 0;

  /// Read the key from ConfigTree
  ///
  /// \param key the key value
  /// \param value the output value (uint64_t)
  virtual bool Read(const std::string &key, uint64_t *value) = 0;

  /// Read the key from ConfigTree
  ///
  /// \param key the key value
  /// \param value the output value (int64_t)
  virtual bool Read(const std::string &key, int64_t *value) = 0;

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (string)
  virtual bool Write(const std::string &key, const std::string &value) = 0;

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (uint32_t)
  virtual bool Write(const std::string &key, uint32_t value) = 0;

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (int32_t)
  virtual bool Write(const std::string &key, int32_t value) = 0;

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (uint64_t)
  virtual bool Write(const std::string &key, uint64_t value) = 0;

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (int64_t)
  virtual bool Write(const std::string &key, int64_t value) = 0;

protected:
  /// Mark as don't touch this file
  bool dontread_;
};
} // namespace config

#endif // H_CONFIG_CONFIG_IMPL
