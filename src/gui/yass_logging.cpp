// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "gui/yass_logging.hpp"

#include "core/logging.hpp"

void YASSLog::DoLogRecord(wxLogLevel level,
                          const wxString& msg,
                          const wxLogRecordInfo& info) {
  if (level >= wxLOG_Message) {
    level = wxLOG_Message;
  }
  const char* message = msg.operator const char*();
  switch (level) {
    case wxLOG_Message: {
      LogMessage log(info.filename, info.line, LOG_INFO);
      log.stream() << message;
      break;
    }
    case wxLOG_Warning: {
      LogMessage log(info.filename, info.line, LOG_WARNING);
      log.stream() << message;
    } break;
    case wxLOG_Error: {
      LogMessage log(info.filename, info.line, LOG_ERROR);
      log.stream() << message;
    } break;
    case wxLOG_FatalError: {
      LogMessage log(info.filename, info.line, LOG_FATAL);
      log.stream() << message;
    } break;
    default:
      break;
  }
}
