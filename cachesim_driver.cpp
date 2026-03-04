#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cachesim.hpp"

static void print_help(void);
static int parse_replace_policy(const char *arg, replacement_policy_t *policy_out);
static int parse_prefetch_algo(const char *arg, prefetch_algo_t *pf_out);
static int validate_config(sim_config_t *config);
static void print_cache_config(cache_config_t *cache_config, const char *cache_name);
static void print_statistics(sim_stats_t* stats);

int main(int argc, char **argv) {
    sim_config_t config = DEFAULT_SIM_CONFIG;
    int opt;

    /* Read arguments */
    while(-1 != (opt = getopt(argc, argv, "c:b:s:C:S:P:F:r:Dh"))) {
        switch(opt) {
        case 'c':
            config.l1_config.c = atoi(optarg);
            break;
        case 'b':
            config.l1_config.b = atoi(optarg);
            config.l2_config.b = config.l1_config.b;
            break;
        case 's':
            config.l1_config.s = atoi(optarg);
            break;
        case 'C':
            config.l2_config.c = atoi(optarg);
            break;
        case 'S':
            config.l2_config.s = atoi(optarg);
            break;
        case 'P':
            if (parse_replace_policy(optarg, &config.l2_config.replace_policy)) {
                return 1;
            }
            break;
        case 'F':
            if (parse_prefetch_algo(optarg, &config.l2_config.prefetch_algorithm)) {
                return 1;
            }
            break;
        case 'r':
            config.l2_config.n_markov_rows = atoi(optarg);
            break;
        case 'D':
            config.l2_config.disabled = 1;
            break;
        case 'h':
            /* Fall through */
        default:
            print_help();
            return 0;
        }
    }

    printf("Cache Settings\n");
    printf("--------------\n");
    print_cache_config(&config.l1_config, "L1");
    print_cache_config(&config.l2_config, "L2");
    printf("\n");

    if (validate_config(&config)) {
        return 1;
    }

    /* Setup the cache */
    sim_setup(&config);

    /* Setup statistics */
    sim_stats_t stats;
    memset(&stats, 0, sizeof stats);

    /* Begin reading the file */
    char rw;
    uint64_t address;
    
    while (!feof(stdin)) {
        int ret = fscanf(stdin, "%c 0x%" PRIx64 "\n", &rw, &address);
        if(ret == 2) {
            sim_access(rw, address, &stats);
        }
    }

    sim_finish(&stats);

    print_statistics(&stats);

    return 0;
}

static int parse_replace_policy(const char *arg, replacement_policy_t *policy_out) {
    if (!strcmp(arg, "mip") || !strcmp(arg, "MIP")) {
        *policy_out = REPLACEMENT_POLICY_MIP;
        return 0;
    } else if (!strcmp(arg, "lip") || !strcmp(arg, "LIP")) {
        *policy_out = REPLACEMENT_POLICY_LIP;
        return 0;
    } else {
        printf("Unknown cache insertion/replacement policy '%s'\n", arg);
        return 1;
    }
}
static int parse_prefetch_algo(const char *arg, prefetch_algo_t *pf_out) {
    if (!strcmp(arg, "none") || !strcmp(arg, "NONE")) {
        *pf_out = PREFETCH_NONE;
        return 0;
    } else if (!strcmp(arg, "plus1") || !strcmp(arg, "PLUS1")) {
        // props for reading the code. have a fish ><>
        *pf_out = PREFETCH_PLUS_ONE;
        return 0;
    } else if (!strcmp(arg, "markov") || !strcmp(arg, "MARKOV")) {
        *pf_out = PREFETCH_MARKOV;
        return 0;
    } else if (!strcmp(arg, "hybrid") || !strcmp(arg, "HYBRID")) {
        *pf_out = PREFETCH_HYBRID;
        return 0;
    } else {
        printf("Unknown cache prefetcher algorithm '%s'\n", arg);
        return 1;
    }
}
static void print_help(void) {
    printf("cachesim [OPTIONS] < traces/file.trace\n");
    printf("-h\t\tThis helpful output\n");
    printf("L1 parameters:\n");
    printf("  -c C1\t\tTotal size for L1 in bytes is 2^C1\n");
    printf("  -b B1\t\tSize of each block for L1 in bytes is 2^B1\n");
    printf("  -s S1\t\tNumber of blocks per set for L1 is 2^S1\n");
    printf("L2 parameters:\n");
    printf("  -C C2\t\tTotal size in bytes for L2 is 2^C1\n");
    printf("  -S S2\t\tNumber of blocks per set for L2 is 2^S1\n");
    printf("  -P P2\t\tInsertion policy for L2 (mip, lip)\n");
    printf("  -D   \t\tDisable L2 cache\n");
    printf("L2 prefetching parameters:\n");
    printf("  -F PF\t\tPrefetching policy to use for L2 (none, plus1, markov, hybrid)\n");
    printf("  -r R \t\tNumber of rows in Markov prefetching table (for markov, hybrid policies)\n");
}

static int validate_config(sim_config_t *config) {
    if (config->l1_config.b > 7 || config->l1_config.b < 4) {
        printf("Invalid configuration! The block size must be reasonable: 4 <= B <= 7\n");
        return 1;
    }

    if (!config->l2_config.disabled && config->l1_config.s > config->l2_config.s) {
        printf("Invalid configuration! L1 associativity must be less than or equal to L2 associativity\n");
        return 1;
    }

    if (!config->l2_config.disabled && config->l1_config.c >= config->l2_config.c) {
        printf("Invalid configuration! L1 size must be strictly less than L2 size\n");
        return 1;
    }

    if (
        !config->l2_config.disabled
        && (config->l2_config.prefetch_algorithm == PREFETCH_NONE || config->l2_config.prefetch_algorithm == PREFETCH_PLUS_ONE)
        && config->l2_config.n_markov_rows
    ) {
        printf("Invalid configuration! Number of Markov rows should be 0 if not using the Markov or Hybrid prefetching algorithms\n");
        return 1;
    }

    return 0;
}

static const char *replace_policy_str(replacement_policy_t policy) {
    switch (policy) {
        case REPLACEMENT_POLICY_MIP: return "MIP";
        case REPLACEMENT_POLICY_LIP: return "LIP";
        default: return "Unknown policy";
    }
}
static const char *prefetch_algo_str(prefetch_algo_t algo) {
    switch (algo) {
        case PREFETCH_NONE: return "None";
        case PREFETCH_PLUS_ONE: return "+1";
        case PREFETCH_MARKOV: return "Markov";
        case PREFETCH_HYBRID: return "Hybrid";
        default: return "Unknown policy";
    }
}

static void print_cache_config(cache_config_t *cache_config, const char *cache_name) {
    printf("%s ", cache_name);
    bool is_L2 = false;
    if(!strcmp(cache_name, "L2"))
        is_L2 = true;
        
    if (cache_config->disabled) {
        printf("disabled\n");
    } else {
        if(!is_L2)
        {
            printf("(C,B,S): (%" PRIu64 ",%" PRIu64 ",%" PRIu64 "). Replace policy: %s\n",
                cache_config->c, cache_config->b, cache_config->s,
                replace_policy_str(cache_config->replace_policy));
        }
        else
        {
            printf("(C,B,S): (%" PRIu64 ",%" PRIu64 ",%" PRIu64 "). Replace policy: %s. Prefetch algo: %s. Prefetch row count: %" PRIu64 "\n",
                cache_config->c, cache_config->b, cache_config->s,
                replace_policy_str(cache_config->replace_policy),
                prefetch_algo_str(cache_config->prefetch_algorithm),
                cache_config->n_markov_rows);
        }        
    }
}

static void print_statistics(sim_stats_t* stats) {
    printf("Cache Statistics\n");
    printf("----------------\n");
    printf("Reads: %" PRIu64 "\n", stats->reads);
    printf("Writes: %" PRIu64 "\n", stats->writes);
    printf("\n");
    printf("L1 accesses: %" PRIu64 "\n", stats->accesses_l1);
    printf("L1 hits: %" PRIu64 "\n", stats->hits_l1);
    printf("L1 misses: %" PRIu64 "\n", stats->misses_l1);
    printf("L1 hit ratio: %.3f\n", stats->hit_ratio_l1);
    printf("L1 miss ratio: %.3f\n", stats->miss_ratio_l1);
    printf("L1 average access time (AAT): %.3f\n", stats->avg_access_time_l1);
    printf("Write-backs from L1: %" PRIu64 "\n", stats->write_backs_l1);
    printf("\n");
    printf("L2 reads: %" PRIu64 "\n", stats->reads_l2);
    printf("L2 writes: %" PRIu64 "\n", stats->writes_l2);
    printf("L2 read hits: %" PRIu64 "\n", stats->read_hits_l2);
    printf("L2 read misses: %" PRIu64 "\n", stats->read_misses_l2);
    printf("L2 read hit ratio: %.3f\n", stats->read_hit_ratio_l2);
    printf("L2 read miss ratio: %.3f\n", stats->read_miss_ratio_l2);
    printf("L2 average access time (AAT): %.3f\n", stats->avg_access_time_l2);
    printf("\n");
    printf("L2 prefetches issued: %" PRIu64 "\n", stats->prefetches_issued_l2);
    printf("L2 prefetch hits: %" PRIu64 "\n", stats->prefetch_hits_l2);
    printf("L2 prefetch misses: %" PRIu64 "\n", stats->prefetch_misses_l2);
}
