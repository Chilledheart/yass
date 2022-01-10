// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef YASS_LOGGING_H
#define YASS_LOGGING_H

#include <wx/log.h>

class YASSLog : public wxLog {
 public:
  void DoLogRecord(wxLogLevel level,
                   const wxString& msg,
                   const wxLogRecordInfo& info) override;
};

#endif  // YASS_LOGGING_H
