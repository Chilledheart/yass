// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */

#include "gui/utils.hpp"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

int32_t Utils::Stoi(const std::string& value) {
  long result = 0;
  char* endptr = nullptr;
  result = strtol(value.c_str(), &endptr, 10);
  if (result > INT32_MAX || (errno == ERANGE && result == LONG_MAX)) {
    wxLogMessage(wxT("overflow: %s"), value.c_str());
    result = 0;
  } else if (result < INT_MIN || (errno == ERANGE && result == LONG_MIN)) {
    wxLogMessage(wxT("undeflow: %s"), value.c_str());
    result = 0;
  } else if (*endptr != '\0') {
    wxLogMessage(wxT("invalid int value: %s"), value.c_str());
    result = 0;
  }
  return result;
}

std::string Utils::ToString(const wxString& value) {
  char cstring[1024];
  strncpy(cstring, value.mb_str(wxConvUTF8), sizeof(cstring) - 1);
  return cstring;
}
