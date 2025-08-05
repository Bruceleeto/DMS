#ifndef PERF_PROFILE_H
#define PERF_PROFILE_H

#include <kos.h>
#include <dc/perf_monitor.h>

typedef struct {
    const char* name;
    uint64_t cycles;
    uint64_t cache_misses;
    uint32_t calls;
    uint64_t min_cycles;
    uint64_t max_cycles;
} perf_profile_t;

typedef struct {
    perf_profile_t transform;
    perf_profile_t clipping;
    perf_profile_t vertex_submit;
    perf_profile_t total_frame;
    perf_profile_t animation;
    perf_profile_t header_setup;
    perf_profile_t pvr_wait;
    perf_profile_t scene_begin;
    perf_profile_t scene_finish;
    perf_profile_t matrix_ops;
} render_profiles_t;

extern render_profiles_t g_profiles;

#define PROFILE_START_CYCLES() \
    uint64_t _prof_start = perf_cntr_count(PRFC1)

#define PROFILE_END_CYCLES(profile) \
    do { \
        uint64_t _prof_end = perf_cntr_count(PRFC1); \
        uint64_t _prof_delta = _prof_end - _prof_start; \
        (profile).cycles += _prof_delta; \
        (profile).calls++; \
        if ((profile).min_cycles == 0 || _prof_delta < (profile).min_cycles) \
            (profile).min_cycles = _prof_delta; \
        if (_prof_delta > (profile).max_cycles) \
            (profile).max_cycles = _prof_delta; \
    } while(0)

// Initialize profiling
void perf_profile_init(void);

// Print profiling results
void perf_profile_print(void);

// Reset all counters
void perf_profile_reset(void);

// Profile cache misses for a code block
void perf_profile_cache_start(void);
uint64_t perf_profile_cache_end(void);

// Advanced profiling modes
void perf_profile_switch_mode(perf_cntr_event_t mode);

#endif
