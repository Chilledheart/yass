// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#ifdef __linux__

#include "core/utils.hpp"

#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <syscall.h>  // For syscall.
#include <time.h>
#include <unistd.h>

#include <filesystem>

#include "core/logging.hpp"
#include "core/process_utils.hpp"
#include "core/utils_fs.hpp"

using std::filesystem::path;
using namespace yass;

static constexpr const struct sched_param kRealTimeAudioPrio = {8};
static constexpr const struct sched_param kRealTimeDisplayPrio = {6};

namespace {

#if BUILDFLAG(IS_LINUX)

const path kCgroupDirectory = "/sys/fs/cgroup";

path ThreadPriorityToCgroupDirectory(const path& cgroup_filepath, ThreadPriority priority) {
  switch (priority) {
    case ThreadPriority::BACKGROUND:
      return cgroup_filepath / "non-urgent";
    case ThreadPriority::NORMAL:
      return cgroup_filepath;
    case ThreadPriority::ABOVE_NORMAL:
      return cgroup_filepath;
    case ThreadPriority::TIME_CRITICAL:
      return cgroup_filepath / "urgent";
  }
  NOTREACHED();
  return path();
}

void SetThreadCgroup(pid_t thread_id, const path& cgroup_directory) {
  path tasks_filepath = cgroup_directory / "tasks";
  std::string tid = std::to_string(thread_id);
  // TODO(crbug.com/1333521): Remove cast.
  const int size = static_cast<int>(tid.size());
  int bytes_written = WriteFileWithBuffer(tasks_filepath, std::string_view(tid.data(), size));
  if (bytes_written != size) {
    LOG(WARNING) << "Failed to add " << tid << " to " << tasks_filepath;
  }
}

void SetThreadCgroupForThreadPriority(pid_t thread_id, const path& cgroup_filepath, ThreadPriority priority) {
  // Append "yass" suffix.
  path cgroup_directory = ThreadPriorityToCgroupDirectory(cgroup_filepath / "yass", priority);

  // Silently ignore request if cgroup directory doesn't exist.
  if (!IsDirectory(cgroup_directory))
    return;

  SetThreadCgroup(thread_id, cgroup_directory);
}

void SetThreadCgroupsForThreadPriority(pid_t thread_id, ThreadPriority priority) {
  path cgroup_filepath(kCgroupDirectory);
  SetThreadCgroupForThreadPriority(thread_id, cgroup_filepath / "cpuset", priority);
  SetThreadCgroupForThreadPriority(thread_id, cgroup_filepath / "schedtune", priority);
}

struct ThreadPriorityToNiceValuePair {
  ThreadPriority priority;
  int nice_value;
};

// These nice values are shared with ChromeOS platform code
// (platform_thread_cros.cc) and have to be unique as ChromeOS has a unique
// type -> nice value mapping. An exception is kCompositing and
// kDisplayCritical where aliasing is OK as they have the same scheduler
// attributes (cpusets, latency_sensitive etc) including nice value.
// The uniqueness of the nice value per-type helps to change and restore the
// scheduling params of threads when their process toggles between FG and BG.
const ThreadPriorityToNiceValuePair kThreadPriorityToNiceValueMap[4] = {
    {ThreadPriority::BACKGROUND, 10},
    {ThreadPriority::NORMAL, 0},
    {ThreadPriority::ABOVE_NORMAL, -8},
    {ThreadPriority::TIME_CRITICAL, -10},
};

int ThreadPriorityToNiceValue(ThreadPriority priority) {
  for (const auto& pair : kThreadPriorityToNiceValueMap) {
    if (pair.priority == priority)
      return pair.nice_value;
  }
  NOTREACHED() << "Unknown ThreadType";
  return 0;
}

void SetThreadPriority(pid_t process_id, pid_t thread_id, ThreadPriority priority) {
  SetThreadCgroupsForThreadPriority(thread_id, priority);

  // Some scheduler syscalls require thread ID of 0 for current thread.
  // This prevents us from requiring to translate the NS TID to
  // global TID.
  pid_t syscall_tid = thread_id;
  if (thread_id == GetTID()) {
    syscall_tid = 0;
  }

  if (priority == ThreadPriority::TIME_CRITICAL) {
    if (sched_setscheduler(syscall_tid, SCHED_RR, &kRealTimeAudioPrio) == 0) {
      return;
    }
    // If failed to set to RT, fallback to setpriority to set nice value.
    PLOG(ERROR) << "Failed to set realtime priority for thread " << thread_id;
  }

  const int nice_setting = ThreadPriorityToNiceValue(priority);
  if (setpriority(PRIO_PROCESS, static_cast<id_t>(syscall_tid), nice_setting)) {
    VPLOG(1) << "Failed to set nice value of thread (" << thread_id << ") to " << nice_setting;
  }
}

#endif  // BUILDFLAG(IS_LINUX)

}  // namespace

bool SetCurrentThreadPriority(ThreadPriority priority) {
#if BUILDFLAG(IS_LINUX)
  SetThreadPriority(getpid(), GetTID(), priority);
#endif  // BUILDFLAG(IS_LINUX)
  return true;
}

bool SetCurrentThreadName(const std::string& name) {
  // On linux we can get the thread names to show up in the debugger by setting
  // the process name for the LWP.  We don't want to do this for the main
  // thread because that would rename the process, causing tools like killall
  // to stop working.
  if (static_cast<pid_t>(pthread_self()) == getpid())
    return true;
  // http://0pointer.de/blog/projects/name-your-threads.html
  // Set the name for the LWP (which gets truncated to 15 characters).
  // Note that glibc also has a 'pthread_setname_np' api, but it may not be
  // available everywhere and it's only benefit over using prctl directly is
  // that it can set the name of threads other than the current thread.
  int err = prctl(PR_SET_NAME, name.c_str());
  // We expect EPERM failures in sandboxed processes, just ignore those.
  if (err < 0 && errno != EPERM)
    PLOG(ERROR) << "prctl(PR_SET_NAME)";
  return err == 0;
}

uint64_t GetMonotonicTime() {
  static struct timespec start_ts;
  static bool started;
  struct timespec ts;
  int ret;
  if (!started) {
    ret = clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts);
    if (ret < 0) {
      PLOG(WARNING) << "clock_gettime failed";
      return 0;
    }
    started = true;
  }
  // Activity to be timed

  ret = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  if (ret < 0) {
    PLOG(WARNING) << "clock_gettime failed";
    return 0;
  }
  return static_cast<double>(ts.tv_sec - start_ts.tv_sec) * NS_PER_SECOND + ts.tv_nsec - start_ts.tv_nsec;
}

bool SetUTF8Locale() {
  if (setlocale(LC_ALL, "C.UTF-8") == nullptr)
    return false;
  if (strcmp(setlocale(LC_ALL, nullptr), "C.UTF-8") != 0)
    return false;
  return true;
}

#endif  // __linux__
