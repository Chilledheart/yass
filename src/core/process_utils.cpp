// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "core/process_utils.hpp"
#include "core/logging.hpp"

#ifndef _WIN32

extern "C" char **environ;

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>

#include <sstream>

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
  // TODO use pipe2 if possible
  // pipe2(xxx, O_CLOEXEC | O_NONBLOCK);
  if ((ret = pipe(stdin_pipe))) {
    PLOG(WARNING) << "pipe failure";
    return ret;
  }
  fcntl(stdin_pipe[0], F_SETFD, FD_CLOEXEC);
  fcntl(stdin_pipe[1], F_SETFD, FD_CLOEXEC);
  if ((ret = pipe(stdout_pipe))) {
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    PLOG(WARNING) << "pipe failure";
    return ret;
  }
  fcntl(stdout_pipe[0], F_SETFD, FD_CLOEXEC);
  fcntl(stdout_pipe[1], F_SETFD, FD_CLOEXEC);
  if ((ret = pipe(stderr_pipe))) {
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);
    PLOG(WARNING) << "pipe failure";
    return ret;
  }
  fcntl(stderr_pipe[0], F_SETFD, FD_CLOEXEC);
  fcntl(stderr_pipe[1], F_SETFD, FD_CLOEXEC);

  ret = pid = fork();
  if (ret < 0) {
    PLOG(WARNING) << "fork failure";
    return ret;
  }
  // In Child Process
  if (ret == 0) {
    // TODO use dup3 if possible
    // dup3(xxx, yyy, O_CLOEXEC);
    dup2(stdin_pipe[0], STDIN_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    dup2(stderr_pipe[1], STDERR_FILENO);

    std::vector<char*> _params;
    _params.reserve(params.size() + 1);
    for (const auto& param : params) {
      _params.push_back(const_cast<char*>(param.c_str()));
    }
    _params.push_back(nullptr);
    ret = execvp(_params[0], &_params[0]);
    if (ret < 0) {
      _exit(ret);
    }
    PLOG(FATAL) << "execvp failure on " << command_line;
  }
  // In Parent Process
  DCHECK(pid) << "Invalid pid in parent process";
  close(stdin_pipe[0]);
  close(stdout_pipe[1]);
  close(stderr_pipe[1]);

  // Post Stage
  fcntl(stdin_pipe[1], F_SETFD, FD_CLOEXEC | O_NONBLOCK);
  fcntl(stdout_pipe[0], F_SETFD, FD_CLOEXEC | O_NONBLOCK);
  fcntl(stderr_pipe[0], F_SETFD, FD_CLOEXEC | O_NONBLOCK);
  std::ostringstream stdout_ss, stderr_ss;
  int wstatus;

  // mark write end as eof
  close(stdin_pipe[1]);

  // polling stdout and stderr
  int mfd = std::max(stdout_pipe[0], stderr_pipe[0]) + 1;
  while (true) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(stdout_pipe[0], &rfds);
    FD_SET(stderr_pipe[0], &rfds);
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
      if (ret == 0)
        goto done;
      stdout_ss << absl::string_view(buf, ret);
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
      if (ret == 0)
        goto done;
      stderr_ss << absl::string_view(buf, ret);
    }
  }

done:
  // already closed
  // close(stdin_pipe[1]);
  close(stdout_pipe[0]);
  close(stderr_pipe[0]);

  if (ret) {
    LOG(INFO) << "process " << command_line << " killed with SIGKILL";
    kill(pid, SIGKILL);
  }
  ret = waitpid(pid, &wstatus, 0);
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
