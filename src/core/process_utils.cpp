// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "core/process_utils.hpp"
#include "core/logging.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(OS_LINUX)
#include <syscall.h>  // For syscall.
#endif

#ifndef _WIN32

extern "C" char **environ;

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>

#include <sstream>

#include <base/posix/eintr_wrapper.h>

static int Pipe2(int pipe_fds[2]) {
  int ret;
#ifdef HAVE_PIPE2
  if ((ret = pipe2(pipe_fds, O_CLOEXEC))) {
    PLOG(WARNING) << "pipe2 failure";
    return ret;
  }
#else
  if ((ret = pipe(pipe_fds))) {
    PLOG(WARNING) << "pipe failure";
    return ret;
  }
  fcntl(pipe_fds[0], F_SETFD, FD_CLOEXEC);
  fcntl(pipe_fds[1], F_SETFD, FD_CLOEXEC);
#endif
  return ret;
}

int ExecuteProcess(const std::vector<std::string>& params,
                   std::string* output,
                   std::string* error) {
  DCHECK(!params.empty()) << "ExecuteProcess empty parameters";
  output->clear();
  error->clear();
  std::string command_line = "'";
  for (const auto &param : params) {
    command_line += param;
    command_line += " ";
  }
  command_line[command_line.size() - 1] = '\'';

  int ret;
  pid_t pid;
  int stdin_pipe[2];
  int stdout_pipe[2];
  int stderr_pipe[2];
  if ((ret = Pipe2(stdin_pipe))) {
    return ret;
  }
  if ((ret = Pipe2(stdout_pipe))) {
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    return ret;
  }
  if ((ret = Pipe2(stderr_pipe))) {
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    return ret;
  }

  ret = pid = fork();
  if (ret < 0) {
    PLOG(WARNING) << "fork failure";
    return ret;
  }
  // In Child Process
  if (ret == 0) {
#ifdef HAVE_DUP3
    dup3(stdin_pipe[0], STDIN_FILENO, 0);
    dup3(stdout_pipe[1], STDOUT_FILENO, 0);
    dup3(stderr_pipe[1], STDERR_FILENO, 0);
#else
    dup2(stdin_pipe[0], STDIN_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    dup2(stderr_pipe[1], STDERR_FILENO);
#endif

    std::vector<char*> _params;
    _params.reserve(params.size() + 1);
    for (const auto& param : params) {
      _params.push_back(const_cast<char*>(param.c_str()));
    }
    _params.push_back(nullptr);
    ret = execvp(_params[0], &_params[0]);

    fprintf(stderr, "execvp failure on %s\n", command_line.c_str());
    fflush(stderr);
    PLOG(ERROR) << "execvp failure on " << command_line;

    if (ret < 0) {
      _exit(ret);
    }
    LOG(FATAL) << "non reachable";
  }
  // In Parent Process
  DCHECK(pid) << "Invalid pid in parent process";
  IGNORE_EINTR(close(stdin_pipe[0]));
  IGNORE_EINTR(close(stdout_pipe[1]));
  IGNORE_EINTR(close(stderr_pipe[1]));

  // Post Stage
  fcntl(stdin_pipe[1], F_SETFD, FD_CLOEXEC | O_NONBLOCK);
  fcntl(stdout_pipe[0], F_SETFD, FD_CLOEXEC | O_NONBLOCK);
  fcntl(stderr_pipe[0], F_SETFD, FD_CLOEXEC | O_NONBLOCK);
  std::ostringstream stdout_ss, stderr_ss;
  int wstatus;

  // mark write end as eof
  IGNORE_EINTR(close(stdin_pipe[1]));

  // polling stdout and stderr
  int mfd = std::max(stdout_pipe[0], stderr_pipe[0]) + 1;
  bool stdout_eof = false, stderr_eof = false;
  while (true) {
    if (stdout_eof && stderr_eof) {
      break;
    }
    fd_set rfds;
    FD_ZERO(&rfds);
    if (!stdout_eof) {
      FD_SET(stdout_pipe[0], &rfds);
    }
    if (!stderr_eof) {
      FD_SET(stderr_pipe[0], &rfds);
    }
    ret = select(mfd, &rfds, nullptr, nullptr, nullptr);
    if (ret < 0) {
      PLOG(WARNING) << "failure on polling process output: " << command_line;
      goto done;
    }
    DCHECK(ret) << "select returns zero event";
    if (FD_ISSET(stdout_pipe[0], &rfds)) {
      char buf[4096];
      ret = read(stdout_pipe[0], buf, sizeof(buf));
      if (ret < 0 && (errno == EAGAIN && errno == EINTR))
        continue;
      if (ret < 0) {
        PLOG(WARNING) << "read failure on polling process output: " << command_line;
        goto done;
      }
      // EOF
      if (ret == 0) {
        VLOG(2) << "process " << command_line << " stdout eof";
        stdout_eof = true;
        continue;
      }
      stdout_ss << std::string_view(buf, ret);
    }
    if (FD_ISSET(stderr_pipe[0], &rfds)) {
      char buf[4096];
      ret = read(stderr_pipe[0], buf, sizeof(buf));
      if (ret < 0 && (errno == EAGAIN && errno == EINTR))
        continue;
      if (ret < 0) {
        PLOG(WARNING) << "read failure on polling process output: " << command_line;
        goto done;
      }
      // EOF
      if (ret == 0) {
        VLOG(2) << "process " << command_line << " stderr eof";
        stderr_eof = true;
        continue;
      }
      stderr_ss << std::string_view(buf, ret);
    }
  }

done:
  // already closed
  // close(stdin_pipe[1]);
  IGNORE_EINTR(close(stdout_pipe[0]));
  IGNORE_EINTR(close(stderr_pipe[0]));

  if (ret) {
    LOG(INFO) << "process " << command_line << " killed with SIGKILL";
    kill(pid, SIGKILL);
  }
  ret = HANDLE_EINTR(waitpid(pid, &wstatus, 0));
  if (ret >= 0) {
    ret = WEXITSTATUS(wstatus);
    VLOG(1) << "process " << command_line << " exited with ret: " << ret;
  } else {
    PLOG(WARNING) << "waitpid failed on process: " << command_line;
  }

  *output = stdout_ss.str();
  *error = stderr_ss.str();
  return ret;
}

#endif

#ifdef _MSC_VER
static_assert(sizeof(uint32_t) == sizeof(DWORD), "");
#elif defined(_WIN32)
static_assert(sizeof(pid_t) >= sizeof(DWORD), "");
#endif

// Keep the same implementation with chromium

#if defined(OS_LINUX) && !defined(OS_ANDROID)

// Store the thread ids in local storage since calling the SWI can be
// expensive and PlatformThread::CurrentId is used liberally. Clear
// the stored value after a fork() because forking changes the thread
// id. Forking without going through fork() (e.g. clone()) is not
// supported, but there is no known usage. Using thread_local is
// fine here (despite being banned) since it is going to be allowed
// but is blocked on a clang bug for Mac (https://crbug.com/829078)
// and we can't use ThreadLocalStorage because of re-entrancy due to
// CHECK/DCHECKs.
thread_local pid_t g_thread_id = -1;

static void ClearTidCache() {
  g_thread_id = -1;
}

class InitAtFork {
 public:
  InitAtFork() { pthread_atfork(nullptr, nullptr, ClearTidCache); }
};

#endif  // defined(OS_LINUX) && !defined(OS_ANDROID)

pid_t GetPID() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if defined(OS_POSIX)
  return getpid();
#elif defined(OS_WIN)
  return GetCurrentProcessId();
#else
#error "Lack GetPID implementation for host environment"
#endif
}

pid_t GetTID() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if defined(OS_APPLE)
  return pthread_mach_thread_np(pthread_self());
  // On Linux and MacOSX, we try to use gettid().
#elif defined(OS_LINUX) && !defined(OS_ANDROID)
  static InitAtFork init_at_fork;
  if (g_thread_id == -1) {
    g_thread_id = syscall(__NR_gettid);
  } else {
    DCHECK_EQ(g_thread_id, syscall(__NR_gettid))
        << "Thread id stored in TLS is different from thread id returned by "
           "the system. It is likely that the process was forked without going "
           "through fork().";
  }
  return g_thread_id;
#elif defined(OS_ANDROID)
  // Note: do not cache the return value inside a thread_local variable on
  // Android (as above). The reasons are:
  // - thread_local is slow on Android (goes through emutls)
  // - gettid() is fast, since its return value is cached in pthread (in the
  //   thread control block of pthread). See gettid.c in bionic.
  return gettid();
#elif defined(OS_WIN)
  return GetCurrentThreadId();
#else
  // If none of the techniques above worked, we use pthread_self().
  return (pid_t)(uintptr_t)pthread_self();
#endif
}

static pid_t g_main_thread_pid = GetPID();

pid_t GetMainThreadPid() {
  return g_main_thread_pid;
}

bool PidHasChanged() {
  pid_t pid = GetPID();
  if (g_main_thread_pid == pid) {
    return false;
  }
  g_main_thread_pid = pid;
  return true;
}

