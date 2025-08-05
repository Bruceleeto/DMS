#include "perf_profile.h"
#include <stdio.h>

render_profiles_t g_profiles = {0};

void perf_profile_init(void) {
    perf_cntr_stop(PRFC1);
    perf_cntr_clear(PRFC1);
    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    
    g_profiles.transform.name = "Vertex Transform";
    g_profiles.clipping.name = "Near-plane Clipping";
    g_profiles.vertex_submit.name = "Vertex Submit";
    g_profiles.total_frame.name = "Total Frame";
    g_profiles.animation.name = "Skeletal Animation";
    g_profiles.header_setup.name = "Header Setup";
}

void perf_profile_print(void) {
    printf("\n=== Performance Profile ===\n");
    
    perf_profile_t* profiles[] = {
        &g_profiles.total_frame,
        &g_profiles.animation,
        &g_profiles.transform,
        &g_profiles.clipping,
        &g_profiles.vertex_submit,
        &g_profiles.header_setup
    };
    
    for (int i = 0; i < 6; i++) {
        perf_profile_t* p = profiles[i];
        if (p->calls > 0) {
            uint64_t avg_cycles = p->cycles / p->calls;
            float ms = (avg_cycles * 5.0f) / 1000000.0f; // 5ns per cycle @ 200MHz
            
            printf("%-20s: %8llu cycles/call (%.3f ms) - %u calls\n",
                   p->name, avg_cycles, ms, p->calls);
        }
    }
    
    if (g_profiles.total_frame.cycles > 0) {
        printf("\n--- Breakdown ---\n");
        uint64_t total = g_profiles.total_frame.cycles / g_profiles.total_frame.calls;
        
        for (int i = 1; i < 6; i++) {
            perf_profile_t* p = profiles[i];
            if (p->calls > 0) {
                uint64_t avg = p->cycles / p->calls;
                float pct = (avg * 100.0f) / total;
                printf("%-20s: %.1f%%\n", p->name, pct);
            }
        }
    }
}

void perf_profile_reset(void) {
    perf_profile_t* profiles[] = {
        &g_profiles.total_frame,
        &g_profiles.animation,
        &g_profiles.transform,
        &g_profiles.clipping,
        &g_profiles.vertex_submit,
        &g_profiles.header_setup
    };
    
    for (int i = 0; i < 6; i++) {
        profiles[i]->cycles = 0;
        profiles[i]->cache_misses = 0;
        profiles[i]->calls = 0;
    }
}

void perf_profile_cache_start(void) {
    perf_cntr_stop(PRFC1);
    perf_cntr_clear(PRFC1);
    perf_cntr_start(PRFC1, PMCR_OPERAND_CACHE_MISS_MODE, PMCR_COUNT_CPU_CYCLES);
}

uint64_t perf_profile_cache_end(void) {
    uint64_t misses = perf_cntr_count(PRFC1);
    perf_cntr_stop(PRFC1);
    perf_cntr_clear(PRFC1);
    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    return misses;
}
