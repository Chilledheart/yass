//
// yass.hpp
// ~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef YASS_APP
#define YASS_APP

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include "worker.hpp"

class YASSFrame;
/// The main Application for Yet-Another-Shadow-Socket
class YASSApp : public wxApp {
public:
  /// On Program Init
  bool OnInit() override;

  void OnStart();
  void OnStop();

  std::string GetStatus() const;

private:
  void LoadConfigFromDisk();
  void SaveConfigToDisk();

private:
  enum { STARTED, STOPPED } state_;

  YASSFrame *frame_;
  Worker worker_;
};

extern YASSApp *mApp;

#endif
