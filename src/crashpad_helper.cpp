// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#include "crashpad_helper.hpp"

#ifdef HAVE_CRASHPAD

#include <build/build_config.h>
#include <filesystem>
#include <map>
#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "client/settings.h"

#include "version.h"

using namespace std::string_literals;

#ifdef __ANDROID__
extern struct android_app* a_app;
extern std::string a_data_dir;
#endif

#if BUILDFLAG(IS_IOS)
static std::unique_ptr<crashpad::CrashReportDatabase> g_database;
bool InitializeCrashpad(const std::string& exe_path) {
  std::filesystem::path tempDir = std::filesystem::temp_directory_path();

  base::FilePath reportsDir(tempDir / "crashpad");

  // Configure url with BugSplat’s public fred database. Replace 'fred' with the name of your BugSplat database.
  std::string url = "https://yass.bugsplat.com/post/bp/crash/crashpad.php"s;

  // Metadata that will be posted to the server with the crash report map
  std::map<std::string, std::string> annotations;
  annotations["format"] = "minidump";                         // Required: Crashpad setting to save crash as a minidump
  annotations["product"] = "yassCrashpadCrasher";             // Required: BugSplat appName
  annotations["version"] = YASS_APP_TAG "-" YASS_APP_SUBTAG;  // Required: BugSplat appVersion
  annotations["key"] = YASS_APP_LAST_CHANGE;                  // Optional: BugSplat key field
  annotations["user"] = "yass@bugsplat.com";                  // Optional: BugSplat user email
  annotations["list_annotations"] = "Optional comment";       // Optional: BugSplat crash description

  // Start crash handler
  crashpad::CrashpadClient* client = new crashpad::CrashpadClient();
  bool ret = client->StartCrashpadInProcessHandler(
      reportsDir, url, annotations, crashpad::CrashpadClient::ProcessPendingReportsObservationCallback());
  if (ret) {
    g_database = crashpad::CrashReportDatabase::Initialize(reportsDir);
  }
  return ret;
}
#else
bool InitializeCrashpad(const std::string& exe_path) {
  std::filesystem::path exeDir = std::filesystem::path(exe_path).parent_path();
#ifdef __ANDROID__
  std::filesystem::path tempDir = a_data_dir;
#else
  std::filesystem::path tempDir = std::filesystem::temp_directory_path();
#endif

  // Ensure that handler is shipped with your application
#ifdef _WIN32
  base::FilePath handler(exeDir / "crashpad_handler.exe");
#elif defined(__ANDROID__)
  base::FilePath handler(exeDir / "libcrashpad_handler.so");
#elif defined(__APPLE__)
  base::FilePath handler(exeDir.parent_path() / "Resources" / "crashpad_handler");
#else
  base::FilePath handler(exeDir / "crashpad_handler");
#endif

  // Directory where reports will be saved. Important! Must be writable or crashpad_handler will crash.
  base::FilePath reportsDir(tempDir / "crashpad");

  // Directory where metrics will be saved. Important! Must be writable or crashpad_handler will crash.
  base::FilePath metricsDir(tempDir / "crashpad");

  // Configure url with BugSplat’s public fred database. Replace 'fred' with the name of your BugSplat database.
  std::string url = "https://yass.bugsplat.com/post/bp/crash/crashpad.php"s;

  // Metadata that will be posted to the server with the crash report map
  std::map<std::string, std::string> annotations;
  annotations["format"] = "minidump";                         // Required: Crashpad setting to save crash as a minidump
  annotations["product"] = "yassCrashpadCrasher";             // Required: BugSplat appName
  annotations["version"] = YASS_APP_TAG "-" YASS_APP_SUBTAG;  // Required: BugSplat appVersion
  annotations["key"] = YASS_APP_LAST_CHANGE;                  // Optional: BugSplat key field
  annotations["user"] = "yass@bugsplat.com";                  // Optional: BugSplat user email
  annotations["list_annotations"] = "Optional comment";       // Optional: BugSplat crash description

  // Disable crashpad rate limiting so that all crashes have dmp files
  std::vector<std::string> arguments;
  arguments.push_back("--no-rate-limit");

  // Initialize Crashpad database
  std::unique_ptr<crashpad::CrashReportDatabase> database = crashpad::CrashReportDatabase::Initialize(reportsDir);
  if (database == NULL)
    return false;

#if 0
  // Enable automated crash uploads
  crashpad::Settings *settings = database->GetSettings();
  if (settings == NULL) return false;
  settings->SetUploadsEnabled(true);
#endif

  // Start crash handler
  crashpad::CrashpadClient* client = new crashpad::CrashpadClient();
  bool status = client->StartHandler(handler, reportsDir, metricsDir, url, annotations, arguments, true, true);
  return status;
}

#endif

#endif
