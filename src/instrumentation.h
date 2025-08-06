#pragma once

#ifdef INSTRUMENTATION_ENABLED

#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/print_string.hpp>

#ifndef INSTRUMENTATION_THRESHOLD_MS
#define INSTRUMENTATION_THRESHOLD_MS 0
#endif

#define INSTRUMENT_FUNCTION_START(func_name) \
    godot::Time *__instrumentation_time = godot::Time::get_singleton(); \
    uint64_t __start_time_usec = __instrumentation_time->get_ticks_usec(); \
    const char *__func_name = func_name;

#define INSTRUMENT_FUNCTION_END() \
    do { \
        uint64_t __end_time_usec = __instrumentation_time->get_ticks_usec(); \
        double __duration_ms = (__end_time_usec - __start_time_usec) / 1000.0; \
        if (__duration_ms >= INSTRUMENTATION_THRESHOLD_MS) { \
            godot::print_line(godot::String("NodeSerializer::") + __func_name + " took " + godot::String::num(__duration_ms, 3) + "ms"); \
        } \
    } while(0)

#else

#define INSTRUMENT_FUNCTION_START(func_name)
#define INSTRUMENT_FUNCTION_END()

#endif
