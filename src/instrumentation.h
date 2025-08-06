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
    const char *__func_name = func_name; \
    godot::StringName __context = godot::StringName();

#define INSTRUMENT_FUNCTION_START_WITH_CONTEXT(func_name, context) \
    godot::Time *__instrumentation_time = godot::Time::get_singleton(); \
    uint64_t __start_time_usec = __instrumentation_time->get_ticks_usec(); \
    const char *__func_name = func_name; \
    godot::StringName __context = context;

#define INSTRUMENT_FUNCTION_START_WITH_SERIALIZATION_CONTEXT(func_name, serialized_value, p_context) \
    godot::String __type_name = godot::Variant::get_type_name(serialized_value.get_type()); \
    godot::String __context_info = p_context.property_name.is_empty() ? __type_name : godot::String(p_context.property_name) + ":" + __type_name; \
    godot::Time *__instrumentation_time = godot::Time::get_singleton(); \
    uint64_t __start_time_usec = __instrumentation_time->get_ticks_usec(); \
    const char *__func_name = func_name; \
    godot::StringName __context = __context_info;

#define INSTRUMENT_FUNCTION_END() \
    do { \
        uint64_t __end_time_usec = __instrumentation_time->get_ticks_usec(); \
        double __duration_ms = (__end_time_usec - __start_time_usec) / 1000.0; \
        if (__duration_ms >= INSTRUMENTATION_THRESHOLD_MS) { \
            godot::String context_str = __context.is_empty() ? godot::String("") : godot::String(" [") + godot::String(__context) + godot::String("]"); \
            godot::print_line(godot::String("NodeSerializer::") + __func_name + context_str + " took " + godot::String::num(__duration_ms, 3) + "ms"); \
        } \
    } while(0)

#else

#define INSTRUMENT_FUNCTION_START(func_name)
#define INSTRUMENT_FUNCTION_START_WITH_CONTEXT(func_name, context)
#define INSTRUMENT_FUNCTION_START_WITH_SERIALIZATION_CONTEXT(func_name, serialized_value, p_context)
#define INSTRUMENT_FUNCTION_END()

#endif
