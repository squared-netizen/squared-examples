// demo — development log.
//
// This file is YOURS. The generator will not overwrite it.
//
// trace() sends one line to the device's own log port when the app is built
// with `make DEV_LOG=1`, and is nothing at all otherwise. Read it from Termux
// with `nc -klu 5555`. It exists because a phone has no console: the way to
// learn what a screen did is to have it say so.

#ifndef demo_TRACE_HPP
#define demo_TRACE_HPP

namespace demo {

#ifdef SQUARED_DEV_LOG
/// printf-style; a line longer than the sink's limit is cut.
void trace(const char* format, ...) __attribute__((format(printf, 1, 2)));
#else
inline void trace(const char*, ...) {}
#endif

}  // namespace demo

#endif  // demo_TRACE_HPP
