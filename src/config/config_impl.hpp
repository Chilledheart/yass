// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#ifndef H_CONFIG_CONFIG_IMPL
#define H_CONFIG_CONFIG_IMPL

#include <memory>
#include <string>

#include <absl/flags/declare.h>
#include <absl/strings/string_view.h>

namespace config {

extern std::string g_configfile;

class ConfigImpl {
 public:
  /// Deconstruct the ConfigTree object
  virtual ~ConfigImpl();

  /// Construct the given ConfigTree implementation
  static std::unique_ptr<ConfigImpl> Create();

  /// Set enforce
  ///
  /// \param enforce_read exit if fail to read
  void SetEnforceRead() { enforce_read_ = true; }

  /// Get enforce
  ///
  /// Get exit if fail to read
  bool GetEnforceRead() const { return enforce_read_; }

  /// Open the ConfigTree
  ///
  /// \param dontread don't load from config tree, useful for write-only object
  bool Open(bool dontread);

  /// Close the ConfigTree, useful to flush the ConfigTree into persistent media
  bool Close();

  /// Read the key from ConfigTree
  ///
  /// \param key the key value
  /// \param absl flag
  template <typename T>
  bool Read(const std::string& key, absl::Flag<T>* value);

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param absl flag
  template <typename T>
  bool Write(const std::string& key, const absl::Flag<T>& value);

  /// Dlete the key
  ///
  /// \param key the key value
  bool Delete(const std::string& key);

 protected:
  /// Open the ConfigTree
  ///
  /// \param dontread don't load from config tree, useful for write-only object
  virtual bool OpenImpl(bool dontread) = 0;

  /// Close the ConfigTree, useful to flush the ConfigTree into persistent media
  virtual bool CloseImpl() = 0;

  /// Read the key from ConfigTree
  ///
  /// \param key the key value
  /// \param value the output value (string)
  virtual bool ReadImpl(const std::string& key, std::string* value) = 0;

  /// Read the key from ConfigTree
  ///
  /// \param key the key value
  /// \param value the output value (bool)
  virtual bool ReadImpl(const std::string& key, bool* value) = 0;

  /// Read the key from ConfigTree
  ///
  /// \param key the key value
  /// \param value the output value (uint32_t)
  virtual bool ReadImpl(const std::string& key, uint32_t* value) = 0;

  /// Read the key from ConfigTree
  ///
  /// \param key the key value
  /// \param value the output value (int32_t)
  virtual bool ReadImpl(const std::string& key, int32_t* value) = 0;

  /// Read the key from ConfigTree
  ///
  /// \param key the key value
  /// \param value the output value (uint64_t)
  virtual bool ReadImpl(const std::string& key, uint64_t* value) = 0;

  /// Read the key from ConfigTree
  ///
  /// \param key the key value
  /// \param value the output value (int64_t)
  virtual bool ReadImpl(const std::string& key, int64_t* value) = 0;

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (string)
  bool WriteImpl(const std::string& key, const std::string& value) {
    return WriteImpl(key, absl::string_view(value));
  }

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (c-style string)
  bool WriteImpl(const std::string& key, const char* value) {
    return WriteImpl(key, absl::string_view(value));
  }

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (string_view)
  virtual bool WriteImpl(const std::string& key, absl::string_view value) = 0;

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (bool)
  virtual bool WriteImpl(const std::string& key, bool value) = 0;

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (uint32_t)
  virtual bool WriteImpl(const std::string& key, uint32_t value) = 0;

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (int32_t)
  virtual bool WriteImpl(const std::string& key, int32_t value) = 0;

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (uint64_t)
  virtual bool WriteImpl(const std::string& key, uint64_t value) = 0;

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  /// \param value the value (int64_t)
  virtual bool WriteImpl(const std::string& key, int64_t value) = 0;

  /// Write the key,value into ConfigTree
  ///
  /// \param key the key value
  virtual bool DeleteImpl(const std::string& key) = 0;

 protected:
  /// dontread don't load from config tree, useful for write-only object
  bool dontread_;
  bool enforce_read_ = false;
};
}  // namespace config

#endif  // H_CONFIG_CONFIG_IMPL
