#ifndef CACHESIM_HPP
#define CACHESIM_HPP

#include <cstdint>
#include <stdint.h>
#include <stdbool.h>

// Replacement policy
typedef enum replacement_policy {
    // MRU insertion, LRU eviction
    REPLACEMENT_POLICY_MIP,
    // LRU insertion, LRU eviction
    REPLACEMENT_POLICY_LIP,
} replacement_policy_t;

typedef enum write_strat {
    // Write back, write-allocate
    WRITE_STRAT_WBWA,
    // Write through, write-no-allocate
    WRITE_STRAT_WTWNA,
} write_strat_t;

// Prefetching algorithm
typedef enum prefetch_algo {
    // No prefetcher
    PREFETCH_NONE,
    // +1 Prefetcher
    PREFETCH_PLUS_ONE,
    // Markov prefetcher (refer to PDF)
    PREFETCH_MARKOV,
    // Hybrid prefetcher (refer to PDF)
    PREFETCH_HYBRID
} prefetch_algo_t;

typedef struct cache_config {
    bool disabled;
    // (C,B,S) in the Conte Cache Taxonomy (Patent Pending)
    uint64_t c;
    uint64_t b;
    uint64_t s;
    replacement_policy_t replace_policy;
    write_strat_t write_strat;
    // Prefetching algorithm
    prefetch_algo_t prefetch_algorithm;
    // Number of Markov prefetching table rows
    // (only applies for Markov and Hybrid prefetchers)
    uint64_t n_markov_rows;
} cache_config_t;

typedef struct sim_config {
    cache_config_t l1_config;
    cache_config_t l2_config;
} sim_config_t;

typedef struct sim_stats {
    // Overall
    uint64_t reads;
    uint64_t writes;
    // L1
    uint64_t accesses_l1;
    uint64_t hits_l1;
    uint64_t misses_l1;
    double hit_ratio_l1;
    double miss_ratio_l1;
    double avg_access_time_l1;
    uint64_t write_backs_l1;
    // L2
    uint64_t reads_l2;
    uint64_t writes_l2;
    uint64_t read_hits_l2;
    uint64_t read_misses_l2;
    double read_hit_ratio_l2;
    double read_miss_ratio_l2;
    double avg_access_time_l2;
    // Prefetch
    uint64_t prefetches_issued_l2;
    uint64_t prefetch_hits_l2;
    uint64_t prefetch_misses_l2;
} sim_stats_t;

extern void sim_setup(sim_config_t *config);
extern void sim_access(char rw, uint64_t addr, sim_stats_t* p_stats);
extern void sim_finish(sim_stats_t *p_stats);

// Sorry about the /* comments */. C++11 cannot handle basic C99 syntax,
// unfortunately
static const sim_config_t DEFAULT_SIM_CONFIG = {
    /*.l1_config =*/ {/*.disabled =*/ 0,
                      /*.c =*/ 10, // 1KB Cache
                      /*.b =*/ 6,  // 64-byte blocks
                      /*.s =*/ 1,  // 2-way
                      /*.replace_policy =*/ REPLACEMENT_POLICY_MIP,
                      /*.write_strat =*/ WRITE_STRAT_WBWA,
                      /*.prefetch_algorithm =*/ PREFETCH_NONE,
                      /*.n_markov_rows =*/ 0},

    /*.l2_config =*/ {/*.disabled =*/ 0,
                      /*.c =*/ 15, // 32KB Cache
                      /*.b =*/ 6,  // 64-byte blocks
                      /*.s =*/ 3,  // 8-way
                      /*.replace_policy =*/ REPLACEMENT_POLICY_LIP,
                      /*.write_strat =*/ WRITE_STRAT_WTWNA,
                      /*.prefetch_algorithm =*/ PREFETCH_NONE,
                      /*.n_markov_rows =*/ 0}
};

// Argument to cache_access rw. Indicates a load
static const char READ = 'R';
// Argument to cache_access rw. Indicates a store
static const char WRITE = 'W';

// Consider the below constants for calculating L2
// Miss Penalty when Early Restart (ER) is enabled.
static const double DRAM_AT = 64;
static const double DRAM_AT_PER_WORD = 2;
static const double WORD_SIZE = 8;

// Hit time (HT) for a given cache (L1 or L2)
// is HIT_TIME_CONST + (HIT_TIME_PER_S * S)
static const double L1_HIT_TIME_CONST = 2;
static const double L1_HIT_TIME_PER_S = 0.2;
static const double L2_HIT_TIME_CONST = 8;
static const double L2_HIT_TIME_PER_S = 0.8;

#endif /* CACHESIM_HPP */
