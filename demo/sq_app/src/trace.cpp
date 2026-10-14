#include "trace.hpp"

#ifdef SQUARED_DEV_LOG

#include <squared/dev/dev_log.hpp>

#include <cstdarg>
#include <cstddef>
#include <cstdio>

namespace demo {

void trace(const char* format, ...)
{
    // On the stack, so tracing never allocates: it runs inside the frame loop.
    char line[256];
    va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    if (length <= 0) return;
    const std::size_t used = static_cast<std::size_t>(length) < sizeof(line)
                                 ? static_cast<std::size_t>(length)
                                 : sizeof(line) - 1;
    sq::dev::log_line({line, used});
}

}  // namespace demo

#endif  // SQUARED_DEV_LOG
