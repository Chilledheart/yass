// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_DEBUG_H_
#define CORE_DEBUG_H_

#include <base/debug/alias.h>
#include <stddef.h>

// Make the optimizer think that |var| is aliased. This can be used to inhibit
// three different kinds of optimizations:
//
// Case #1: Prevent a local variable from being optimized out if it would not
// otherwise be live at the point of a potential crash. This can only be done
// with local variables, not globals, object members, or function return values
// - these must be copied to locals if you want to ensure they are recorded in
// crash dumps. Function arguments are fine to use since the
// Alias() call on them will make sure they are copied to the stack
// even if they were passed in a register. Note that if the local variable is a
// pointer then its value will be retained but the memory that it points to will
// probably not be saved in the crash dump - by default only stack memory is
// saved. Therefore the aliasing technique is usually only worthwhile with
// non-pointer variables. If you have a pointer to an object and you want to
// retain the object's state you need to copy the object or its fields to local
// variables.
//
// Example usage:
//   int last_error = err_;
//   Alias(&last_error);
//   DEBUG_ALIAS_FOR_CSTR(name_copy, p->name, 16);
//   CHECK(false);
//
// Case #2: Prevent a tail call into a function. This is useful to make sure the
// function containing the call to Alias() will be present in the
// call stack. In this case there is no memory that needs to be on
// the stack so we can use nullptr. The call to Alias() needs to
// happen after the call that is suspected to be tail called. Note: This
// technique will prevent tail calls at the specific call site only. To prevent
// them for all invocations of a function look at NOT_TAIL_CALLED.
//
// Example usage:
//   NOINLINE void Foo(){
//     ... code ...
//
//     Bar();
//     Alias(nullptr);
//   }
//
// Case #3: Prevent code folding of a non-unique function. Code folding can
// cause the same address to be assigned to different functions if they are
// identical. If finding the precise signature of a function in the call-stack
// is important and it's suspected the function is identical to other functions
// it can be made unique using NO_CODE_FOLDING which is a wrapper around
// Alias();
//
// Example usage:
//   NOINLINE void Foo(){
//     NO_CODE_FOLDING();
//     Bar();
//   }
//
// Finally please note that these effects compound. This means that saving a
// stack variable (case #1) using Alias() will also inhibit
// tail calls for calls in earlier lines and prevent code folding.

void Alias(const void* var);

namespace internal {
size_t strlcpy(char* dst, const char* src, size_t dst_size);
}  // namespace internal

// Code folding is a linker optimization whereby the linker identifies functions
// that are bit-identical and overlays them. This saves space but it leads to
// confusing call stacks because multiple symbols are at the same address and
// it is unpredictable which one will be displayed. Disabling of code folding is
// particularly useful when function names are used as signatures in crashes.
// This macro doesn't guarantee that code folding will be prevented but it
// greatly reduces the odds and always prevents it within one source file.
// If using in a function that terminates the process it is safest to put the
// NO_CODE_FOLDING macro at the top of the function.
// Use like:
//   void FooBarFailure(size_t size) { NO_CODE_FOLDING(); OOM_CRASH(size); }
#define NO_CODE_FOLDING()           \
  const int line_number = __LINE__; \
  Alias(&line_number)

// Waits wait_seconds seconds for a debugger to attach to the current process.
// When silent is false, an exception is thrown when a debugger is detected.
bool WaitForDebugger(int wait_seconds, bool silent);

// Returns true if the given process is being run under a debugger.
//
// On OS X, the underlying mechanism doesn't work when the sandbox is enabled.
// To get around this, this function caches its value.
//
// WARNING: Because of this, on OS X, a call MUST be made to this function
// BEFORE the sandbox is enabled.
bool BeingDebugged();

// Break into the debugger, assumes a debugger is present.
void BreakDebugger();
// Async-safe version of BreakDebugger(). In particular, this does not allocate
// any memory. More broadly, must be safe to call from anywhere, including
// signal handlers.
void BreakDebuggerAsyncSafe();

// Used in test code, this controls whether showing dialogs and breaking into
// the debugger is suppressed for debug errors, even in debug mode (normally
// release mode doesn't do this stuff --  this is controlled separately).
// Normally UI is not suppressed.  This is normally used when running automated
// tests where we want a crash rather than a dialog or a debugger.
void SetSuppressDebugUI(bool suppress);
bool IsDebugUISuppressed();

// If a debugger is present, verifies that it is properly set up, and DCHECK()s
// if misconfigured.  Currently only verifies that //tools/gdb/gdbinit has been
// sourced when using gdb on Linux and //tools/lldb/lldbinit.py has been sourced
// when using lldb on macOS.
void VerifyDebugger();

#endif  // CORE_DEBUG_H_
