#include <errno.h>
#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include <windows.h>
#include <process.h>
#include <shellapi.h>

// Create a shim to lld-link.exe,
// from llvm-lib.exe <ARGS>
//   to lld-link.exe /lib <ARGS>

// Silient failed with no reason
#if 0
int wmain(int argc, wchar_t *argv[]) {
  memcpy(wcsrchr(argv[0], '\\') + 1, L"lld-link.exe", sizeof(L"lld-link.exe"));
  const wchar_t** shim_argv = (const wchar_t**)malloc((argc + 2) * sizeof(wchar_t*));
  shim_argv[0] = argv[0];
  shim_argv[1] = L"/lib";
  memcpy(shim_argv + 2, argv + 1, sizeof(wchar_t*) * (argc - 1));
  shim_argv[argc + 1] = L"\0";
  if (_wexecvp(shim_argv[0], shim_argv) < 0) {
    fprintf(stderr, "Failed to execute cmd %ls due to error: %d\n",
            shim_argv[0], errno);
    return -1;
  }
  // Unlikely code path
  return 0;
}
#else
static HANDLE hProcess;
static BOOL WINAPI NotifyInterrupted(DWORD dwCtrlType) {
  if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
    /* deal with subprocess */
    if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, GetProcessId(hProcess))) {
      fprintf(stderr, "Failed to send CTRL_BREAK_EVENT to subprocess due to error: %ld\n",
              GetLastError());
      return FALSE;
    }
    return TRUE;
  }
  return FALSE;
}
// It works
int wmain() {
  // Generate exe_path (aka lld-link.exe)
  wchar_t exe_path[MAX_PATH + 1] = {};
  GetModuleFileNameW(NULL, exe_path, MAX_PATH);
  memcpy(wcsrchr(exe_path, '\\') + 1, L"lld-link.exe", sizeof(L"lld-link.exe"));

  // Generate shim_argv from cmdline
  wchar_t *cmdline = GetCommandLineW();
  int argc;
  wchar_t** argv = CommandLineToArgvW(cmdline, &argc);
  const wchar_t** shim_argv = (const wchar_t**)malloc((argc + 2) * sizeof(wchar_t*));
  if (!shim_argv) {
    fprintf(stderr, "Out of memory\n");
    return -1;
  }
  shim_argv[0] = exe_path;
  shim_argv[1] = L"/lib";
  memcpy(shim_argv + 2, argv + 1, sizeof(wchar_t*) * (argc - 1));
  shim_argv[argc + 1] = L"\0";

  // Convert shim_argv to shim_cmdline
  wchar_t* shim_cmdline =
      (wchar_t*)malloc(UNICODE_STRING_MAX_CHARS /*32767*/ * sizeof(wchar_t));
  if (!shim_cmdline) {
    fprintf(stderr, "Out of memory\n");
    return -1;
  }
  wchar_t *pos = shim_cmdline;
  for (int i = 0; i < argc + 1; ++i) {
    memcpy(pos, L"\"", sizeof(wchar_t));
    ++pos;
    int frag_len = wcslen(shim_argv[i]);
    memcpy(pos, shim_argv[i], frag_len * sizeof(wchar_t));
    pos += frag_len;
    memcpy(pos, L"\"", sizeof(wchar_t));
    ++pos;
    if (i != argc) {
      memcpy(pos, L" ", sizeof(wchar_t));
      ++pos;
    }
  }
  *pos = L'\0';

  // Start process
  STARTUPINFOW startupInfo = {};
  startupInfo.cb = sizeof(STARTUPINFO);
  startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  startupInfo.dwFlags |= STARTF_USESTDHANDLES;

  PROCESS_INFORMATION processInformation = {};

  if (!CreateProcessW(exe_path, shim_cmdline, NULL, NULL, TRUE,
                      NORMAL_PRIORITY_CLASS, NULL, NULL, &startupInfo,
                      &processInformation)) {
    fprintf(stderr, "Failed to execute cmd %ls due to error: %ld\n",
            shim_cmdline, GetLastError());
    return -1;
  }

  hProcess = processInformation.hProcess;

  DWORD dw_exit_code;

  if (!GetExitCodeProcess(processInformation.hProcess, &dw_exit_code)) {
    fprintf(stderr, "Failed to get subprocess exit code due to error: %ld\n",
            GetLastError());
    return -1;
  }

  // Let's wait for the process to finish.
  if (dw_exit_code == STILL_ACTIVE) {
    // Handle with interrupt
    if (!SetConsoleCtrlHandler(NotifyInterrupted, TRUE)) {
      fprintf(stderr, "Failed to register interrupt handler due to error: %ld\n",
              GetLastError());
      return -1;
    }

    if (WaitForSingleObject(processInformation.hProcess, INFINITE) != WAIT_OBJECT_0) {
      fprintf(stderr, "Failed to wait subprocess due to error: %ld\n",
              GetLastError());
      return -1;
    }

    // Reset Handle with interrupt
    SetConsoleCtrlHandler(NotifyInterrupted, FALSE);

    if (!GetExitCodeProcess(processInformation.hProcess, &dw_exit_code)) {
      fprintf(stderr, "Failed to get subprocess exit code due to error: %ld\n",
              GetLastError());
      return -1;
    }
  }

  // Unlikely code path
  if (dw_exit_code == STILL_ACTIVE) {
    fprintf(stderr, "Killing pending subprocess\n");
    TerminateProcess(processInformation.hProcess, 1);
  }

  CloseHandle(processInformation.hThread);
  CloseHandle(processInformation.hProcess);

  return (int)dw_exit_code;
}
#endif
