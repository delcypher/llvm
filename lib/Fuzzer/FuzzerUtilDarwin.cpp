//===- FuzzerUtilDarwin.cpp - Misc utils ----------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Misc utils for Darwin.
//===----------------------------------------------------------------------===//
#include "FuzzerInternal.h"
#if LIBFUZZER_APPLE
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>

// There is no header for this on macOS so declare here
extern char **environ;
namespace fuzzer {
/* This is a reimplementation of Libc's `system()`.
 * On Darwin the Libc implementation contains a mutex
 * which prevents it from being used in parallel. This implementation
 * does not lock and so can be used in parallel. However unlike
 * `system()` this method does not try to modify signal handlers
 * of the current process to avoid being racey.
 */
int ExecuteCommand(const std::string &Command) {
  const char *CommandCStr = Command.c_str();
  pid_t Pid;
  int ErrorCode = 0, ProcessStatus = 0;
  char **Environ = environ; // Read from global
  const char *Argv[] = {"sh", "-c", CommandCStr, NULL};
  sigset_t DefaultSigSet;
  posix_spawnattr_t SpawnAttributes;
  short SpawnFlags = POSIX_SPAWN_SETSIGDEF;
  sigemptyset(&DefaultSigSet);
  if (posix_spawnattr_init(&SpawnAttributes))
    return -1;

  // Make sure the child process uses the default handlers for the
  // following signals rather than inheriting what the parent has.
  sigaddset(&DefaultSigSet, SIGQUIT);
  sigaddset(&DefaultSigSet, SIGINT);
  posix_spawnattr_setsigdefault(&SpawnAttributes, &DefaultSigSet);
  posix_spawnattr_setflags(&SpawnAttributes, SpawnFlags);

  // FIXME: We probably shouldn't hardcode the shell path.
  ErrorCode = posix_spawn(&Pid, "/bin/sh", NULL, &SpawnAttributes,
                          (char *const *)Argv, Environ);
  posix_spawnattr_destroy(&SpawnAttributes);
  if (!ErrorCode) {
    pid_t SavedPid = Pid;
    do {
      // Repeat until call completes uninterrupted.
      Pid = waitpid(SavedPid, &ProcessStatus, /*options=*/0);
    } while (Pid == -1 && errno == EINTR);
    if (Pid == -1) {
      // Fail for some other reason.
      ProcessStatus = -1;
    }
  } else if (ErrorCode == ENOMEM || ErrorCode == EAGAIN) {
    // Fork failure.
    ProcessStatus = -1;
  } else {
    // Shell execution failure.
    ProcessStatus = W_EXITCODE(127, 0);
  }
  return ProcessStatus;
}
}
#endif // LIBFUZZER_APPLE
