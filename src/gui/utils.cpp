// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */

#include "gui/utils.hpp"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

int32_t Utils::Stoi(const std::string &value) {
  int32_t result = 0;
  try {
    result = std::stoi(value);
  } catch (const std::invalid_argument &) {
    wxLogMessage(wxT("invalid_argument: %s"), value.c_str());
  } catch (const std::out_of_range &) {
    wxLogMessage(wxT("out_of_range: %s"), value.c_str());
  }
  return result;
}

std::string Utils::ToString(const wxString &value) {
  char cstring[1024];
  strncpy(cstring, value.mb_str(wxConvUTF8), sizeof(cstring) - 1);
  return cstring;
}
