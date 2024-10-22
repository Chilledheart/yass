// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#include "core/process_utils.hpp"

#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <base/threading/platform_thread.h>
#include <build/buildflag.h>
#include "core/logging.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#if BUILDFLAG(IS_LINUX)
#include <syscall.h>  // For syscall.
#endif

#ifndef _WIN32

#include <fcntl.h>     // For pipe2 and fcntl
#include <signal.h>    // For kill
#include <sys/wait.h>  // For waitpid
#include <unistd.h>    // For pipe

#include <base/posix/eintr_wrapper.h>
#include <sstream>

#define ASIO_NO_SSL
#include "net/asio.hpp"

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
  if ((ret = fcntl(pipe_fds[0], F_SETFD, FD_CLOEXEC)) != 0 || (ret = fcntl(pipe_fds[1], F_SETFD, FD_CLOEXEC)) != 0) {
    PLOG(WARNING) << "fcntl F_SETFD failure";
    IGNORE_EINTR(close(pipe_fds[0]));
    IGNORE_EINTR(close(pipe_fds[1]));
    return ret;
  }
#endif
  return ret;
}

namespace {

class ProcessInOutReader {
 public:
  ProcessInOutReader(const std::string& command_line, int stdout_pipe, int stderr_pipe)
      : command_line_(command_line), out_(io_context_, stdout_pipe), err_(io_context_, stderr_pipe) {}

  ~ProcessInOutReader() {
    asio::error_code ec;
    out_.close(ec);
    if (ec) {
      LOG(WARNING) << "process " << command_line_ << " close: error: " << ec;
    }
    err_.close(ec);
    if (ec) {
      LOG(WARNING) << "process " << command_line_ << " close: error: " << ec;
    }
  }

  void run() {
    work_guard_ =
        std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context_.get_executor());
    ScheduleStdoutRead();
    ScheduleStderrRead();
    io_context_.run();
  }

  int ret() const { return ret_; }

  std::string out() const { return stdout_ss_.str(); }

  std::string err() const { return stderr_ss_.str(); }

 private:
  void ScheduleStdoutRead() {
    asio::async_read(out_, asio::mutable_buffer(out_buffer_, sizeof(out_buffer_)),
                     [this](asio::error_code ec, size_t bytes_transferred) {
                       if (bytes_transferred) {
                         stdout_ss_ << std::string_view(out_buffer_, bytes_transferred);
                       }
                       if (ec == asio::error::eof) {
                         VLOG(2) << "process " << command_line_ << " reached stdout eof";
                         stdout_eof_ = true;
                         if (stdout_eof_ && stderr_eof_) {
                           work_guard_.reset();
                         }
                         return;
                       }
                       if (ec) {
                         LOG(WARNING) << "process " << command_line_ << " reading stdout error: " << ec;
                         ret_ = -1;
                         work_guard_.reset();
                         return;
                       }
                       ScheduleStdoutRead();
                     });
  }

  void ScheduleStderrRead() {
    asio::async_read(err_, asio::mutable_buffer(err_buffer_, sizeof(err_buffer_)),
                     [this](asio::error_code ec, size_t bytes_transferred) {
                       if (bytes_transferred) {
                         stderr_ss_ << std::string_view(err_buffer_, bytes_transferred);
                       }
                       if (ec == asio::error::eof) {
                         VLOG(2) << "process " << command_line_ << " reached stderr eof";
                         stderr_eof_ = true;
                         if (stdout_eof_ && stderr_eof_) {
                           work_guard_.reset();
                         }
                         return;
                       }
                       if (ec) {
                         LOG(WARNING) << "process " << command_line_ << " reading stderr error: " << ec;
                         ret_ = -1;
                         work_guard_.reset();
                         return;
                       }
                       ScheduleStderrRead();
                     });
  }

 private:
  const std::string& command_line_;
  asio::io_context io_context_;
  asio::posix::stream_descriptor out_;
  asio::posix::stream_descriptor err_;
  int ret_ = 0;
  char out_buffer_[4096];
  bool stdout_eof_ = false;
  char err_buffer_[4096];
  bool stderr_eof_ = false;

  std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;

  std::ostringstream stdout_ss_;
  std::ostringstream stderr_ss_;
};

}  // namespace

int ExecuteProcess(const std::vector<std::string>& params, std::string* output, std::string* error) {
  DCHECK(!params.empty()) << "ExecuteProcess empty parameters";
  output->clear();
  error->clear();
  std::string command_line = absl::StrCat("'", absl::StrJoin(params, " "), "'");

  int ret;
  pid_t pid;
  int stdin_pipe[2];
  int stdout_pipe[2];
  int stderr_pipe[2];
  if ((ret = Pipe2(stdin_pipe))) {
    return ret;
  }
  if ((ret = Pipe2(stdout_pipe))) {
    IGNORE_EINTR(close(stdin_pipe[0]));
    IGNORE_EINTR(close(stdin_pipe[1]));
    return ret;
  }
  if ((ret = Pipe2(stderr_pipe))) {
    IGNORE_EINTR(close(stdin_pipe[0]));
    IGNORE_EINTR(close(stdin_pipe[1]));
    IGNORE_EINTR(close(stdout_pipe[0]));
    IGNORE_EINTR(close(stdout_pipe[1]));
    return ret;
  }

  ret = pid = fork();
  if (ret < 0) {
    PLOG(WARNING) << "fork failure";
    return ret;
  }
  // In Child Process
  if (ret == 0) {
    // The two file descriptors do not share file descriptor flags (the close-on-exec flag)
#ifdef HAVE_DUP3
    if (dup3(stdin_pipe[0], STDIN_FILENO, 0) < 0 || dup3(stdout_pipe[1], STDOUT_FILENO, 0) < 0 ||
        dup3(stderr_pipe[1], STDERR_FILENO, 0) < 0) {
      LOG(FATAL) << "dup3 on std file descriptors failure";
    }
#else
    if (dup2(stdin_pipe[0], STDIN_FILENO) < 0 || dup2(stdout_pipe[1], STDOUT_FILENO) < 0 ||
        dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
      LOG(FATAL) << "dup2 on std file descriptors failure";
    }
#endif

    std::vector<char*> _params;
    _params.reserve(params.size() + 1);
    for (const auto& param : params) {
      _params.push_back(const_cast<char*>(param.c_str()));
    }
    _params.push_back(nullptr);
    ret = execvp(_params[0], &_params[0]);
    PLOG(ERROR) << "execvp failure on " << command_line;

    fprintf(stderr, "execvp failure on %s\n", command_line.c_str());
    fflush(stderr);

    if (ret < 0) {
      _exit(ret);
    }
    LOG(FATAL) << "non reachable: execvp: " << ret;
  }
  // In Parent Process
  DCHECK(pid) << "Invalid pid in parent process";
  IGNORE_EINTR(close(stdin_pipe[0]));
  IGNORE_EINTR(close(stdout_pipe[1]));
  IGNORE_EINTR(close(stderr_pipe[1]));

  // TODO implement write input
  // mark write end as eof
  IGNORE_EINTR(close(stdin_pipe[1]));

  // polling stdout and stderr
  {
    ProcessInOutReader reader(command_line, stdout_pipe[0], stderr_pipe[0]);
    reader.run();

    ret = reader.ret();
    *output = reader.out();
    *error = reader.err();
  }

  if (ret) {
    LOG(INFO) << "process " << command_line << " killed with SIGKILL";
    kill(pid, SIGKILL);
  }
  int wstatus;
  ret = HANDLE_EINTR(waitpid(pid, &wstatus, 0));
  if (ret >= 0) {
    ret = WEXITSTATUS(wstatus);
    VLOG(1) << "process " << command_line << " exited with ret: " << ret;
  } else {
    PLOG(WARNING) << "waitpid failed on process: " << command_line;
  }
  return ret;
}

#endif  // _WIN32

#ifdef _MSC_VER
static_assert(sizeof(uint32_t) == sizeof(DWORD), "");
#elif defined(_WIN32)
static_assert(sizeof(pid_t) >= sizeof(DWORD), "");
#endif

// Keep the same implementation with chromium (see base/process/process_handle_posix.cc)
pid_t GetPID() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if BUILDFLAG(IS_POSIX)
  return getpid();
#elif BUILDFLAG(IS_WIN)
  return GetCurrentProcessId();
#else
#error "Lack GetPID implementation for host environment"
#endif
}

pid_t GetTID() {
  return gurl_base::PlatformThread::CurrentId();
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
