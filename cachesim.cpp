#include "cachesim.hpp"
#include <cmath>
#include <list>
#include <vector>

struct Block {
    uint64_t tag;
    bool dirty;
    bool valid;
    bool prefetched;

    Block(){
        tag = 0;
        dirty = false;
        valid = false;
        prefetched = false;
    }
};

struct MarkovEntry {
    uint64_t next_block_addr;
    uint32_t frequency;
};

struct MarkovRow {
    uint64_t source_addr;
    std::vector<MarkovEntry> entries;
};

std::vector<std::list<Block>> l1_cache;
std::vector<std::list<Block>> l2_cache;
std::list<MarkovRow> markov_table;

uint64_t last_miss_block_addr = 0;
bool first_miss_occurred = false;
sim_config_t *g_config;

// Compute tag and set index for an address given 
void get_cache_parts(uint64_t addr, const cache_config_t &config, uint64_t &tag, uint64_t &index) {
    uint64_t index_bits = config.c - config.b - config.s;
    tag = addr >> (config.b + index_bits);
    index = (addr >> config.b) & ((1ULL << index_bits) - 1);
}

// reconstruct a block address from tag and index for a given cache configuration
uint64_t reconstruct_addr(uint64_t tag, uint64_t index, const cache_config_t &config) {
    uint64_t index_bits = config.c - config.b - config.s;
    return (tag << (index_bits + config.b)) | (index << config.b);
}

// Return true if the block address exists in L1 or L2.
bool is_block_in_any_cache(uint64_t block_addr) {
    uint64_t t, i;
    uint64_t full_addr = block_addr << g_config->l1_config.b;

    get_cache_parts(full_addr, g_config->l1_config, t, i);
    for (auto &b : l1_cache[i]) {
        if (b.valid && b.tag == t){
            return true;
        }
    }

    if (!g_config->l2_config.disabled) {
        get_cache_parts(full_addr, g_config->l2_config, t, i);
        for (auto &b : l2_cache[i]) {
            if (b.valid && b.tag == t){
                return true;
            }
        }
    }
    return false;
}

// Markov table updating using the transition from the previous miss block to the current block.
void update_markov_table(uint64_t current_block_addr) {
    if (!first_miss_occurred) {
        last_miss_block_addr = current_block_addr;
        first_miss_occurred = true;
        return;
    }

    auto it = markov_table.begin();
    for (; it != markov_table.end(); ++it) {
        if (it->source_addr == last_miss_block_addr) break;
    }

    if (it != markov_table.end()) {
        MarkovRow row = *it;
        markov_table.erase(it);

        bool found = false;
        for (auto &e : row.entries) {
            if (e.next_block_addr == current_block_addr) {
                e.frequency++;
                found = true;
                break;
            }
        }

        if (!found) {
            if (row.entries.size() == 4) {
                int lfu = 0;
                for (int k = 1; k < 4; ++k) {
                    if (row.entries[k].frequency < row.entries[lfu].frequency ||
                        (row.entries[k].frequency == row.entries[lfu].frequency &&
                         row.entries[k].next_block_addr < row.entries[lfu].next_block_addr)) {
                        lfu = k;
                    }
                }
                row.entries.erase(row.entries.begin() + lfu);
            }
            row.entries.push_back({current_block_addr, 1});
        }

        markov_table.push_front(row);
    } else {
        
        if (markov_table.size() == g_config->l2_config.n_markov_rows) {
            markov_table.pop_back();
        }

        MarkovRow nr;
        nr.source_addr = last_miss_block_addr;
        nr.entries.push_back({current_block_addr, 1});
        markov_table.push_front(nr);
    }

    last_miss_block_addr = current_block_addr;
}

// Insert a prefetched block into L2 if it is not already present in L1 or L2.
void issue_l2_prefetch(uint64_t pred_addr, sim_stats_t *stats) {
    if (is_block_in_any_cache(pred_addr)) return;

    uint64_t t, i;
    get_cache_parts(pred_addr << g_config->l2_config.b, g_config->l2_config, t, i);
    std::list<Block> &set = l2_cache[i];

    if (set.size() == pow(2,g_config->l2_config.s)) {
        if (set.back().prefetched) stats->prefetch_misses_l2++;
        set.pop_back();
    }

    Block pb;
    pb.tag = t;
    pb.valid = true;
    pb.prefetched = true;

    set.push_back(pb);
    stats->prefetches_issued_l2++;
}

// Run the L2 prefetcher on the current access.
void run_prefetcher(uint64_t addr, sim_stats_t *stats) {
    uint64_t curr_block = addr >> g_config->l1_config.b;

    if (g_config->l2_config.prefetch_algorithm == PREFETCH_PLUS_ONE) {
        issue_l2_prefetch(curr_block + 1, stats);
        return;
    }

    if (g_config->l2_config.prefetch_algorithm >= PREFETCH_MARKOV) {
        auto it = markov_table.begin();
        bool found = false;

        for (; it != markov_table.end(); ++it) {
            if (it->source_addr == curr_block) {
                found = true;
                break;
            }
        }

        if (found && !it->entries.empty()) {
            MarkovEntry best = it->entries[0];
            for (size_t k = 1; k < it->entries.size(); ++k) {
                if (it->entries[k].frequency > best.frequency ||
                    (it->entries[k].frequency == best.frequency &&
                     it->entries[k].next_block_addr > best.next_block_addr)) {
                    best = it->entries[k];
                }
            }
            if (best.next_block_addr != curr_block) {
                issue_l2_prefetch(best.next_block_addr, stats);
            }
        } else if (g_config->l2_config.prefetch_algorithm == PREFETCH_HYBRID) {
            issue_l2_prefetch(curr_block + 1, stats);
        }

        update_markov_table(curr_block);
    }
}

// Perform an access to L2 
void access_l2(char rw, uint64_t addr, sim_stats_t *stats) {
    if (g_config->l2_config.disabled) {
        if (rw == READ) {
            stats->reads_l2++;
            stats->read_misses_l2++;
        } else {
            stats->writes_l2++;
        }
        return;
    }

    uint64_t t, i;
    get_cache_parts(addr, g_config->l2_config, t, i);
    std::list<Block> &set = l2_cache[i];

    if (rw == READ) {
        stats->reads_l2++;
    } else {
        stats->writes_l2++;
    }

    auto it = set.begin();
    bool hit = false;
    for (; it != set.end(); ++it) {
        if (it->valid && it->tag == t) {
            hit = true;
            break;
        }
    }

    if (hit) {
        if (rw == READ) {
            stats->read_hits_l2++;
            if (it->prefetched) {
                stats->prefetch_hits_l2++;
                it->prefetched = false;
            }
        }

        Block b = *it;
        set.erase(it);
        set.push_front(b);
        return;
    }

    if (rw == READ) {
        stats->read_misses_l2++;

        if (set.size() == pow(2,g_config->l2_config.s)) {
            if (set.back().prefetched) stats->prefetch_misses_l2++;
            set.pop_back();
        }

        Block nb;
        nb.tag = t;
        nb.valid = true;
        set.push_back(nb);
        run_prefetcher(addr, stats);
    }
}

// L1 access, forwarding misses to L2 and handling L1 writebacks.
void sim_access(char rw, uint64_t addr, sim_stats_t *stats) {
    if (rw == READ) {
        stats->reads++;
    } else {
        stats->writes++;
    }

    stats->accesses_l1++;

    uint64_t tag, index;
    get_cache_parts(addr, g_config->l1_config, tag, index);
    std::list<Block> &set = l1_cache[index];


    auto it = set.begin();
    bool hit = false;
    for (; it != set.end(); ++it) {
        if (it->valid && it->tag == tag) {
            hit = true;
            break;
        }
    }

    if (hit) {
        stats->hits_l1++;

        if (rw == WRITE){
            it->dirty = true;
        }

        Block b = *it;
        set.erase(it);
        set.push_front(b);
        return;
    }

    stats->misses_l1++;

    access_l2(READ, addr, stats);

    if (set.size() == pow(2,g_config->l1_config.s)) {
        Block victim = set.back();
        set.pop_back();

        if (victim.dirty) {

            stats->write_backs_l1++;
            access_l2(WRITE, reconstruct_addr(victim.tag, index, g_config->l1_config), stats);
        }
    }

    Block nb;
    nb.tag = tag;
    nb.valid = true;
    nb.dirty = (rw == WRITE);
    set.push_front(nb);
}

// Initialize simulator (L1 and L2) with the given configuration
void sim_setup(sim_config_t *config) {

    g_config = config;
    l1_cache.resize(1ULL << (config->l1_config.c - config->l1_config.b - config->l1_config.s));

    if (!config->l2_config.disabled) {
        l2_cache.resize(1ULL << (config->l2_config.c - config->l2_config.b - config->l2_config.s));
    }
}

// compute final statistics after simulation is done
void sim_finish(sim_stats_t *stats) {

    if (stats->accesses_l1 > 0) {
        stats->hit_ratio_l1 = (double)stats->hits_l1 / stats->accesses_l1;
        stats->miss_ratio_l1 = (double)stats->misses_l1 / stats->accesses_l1;
    }

    if (stats->reads_l2 > 0){
        stats->read_miss_ratio_l2 = (double)stats->read_misses_l2 / stats->reads_l2;
        stats->read_hit_ratio_l2 = 1.0 - stats->read_miss_ratio_l2;
    } else if (g_config->l2_config.disabled){
        stats->read_miss_ratio_l2 = 1.0;
    }

    double l1_ht = L1_HIT_TIME_CONST + (L1_HIT_TIME_PER_S * (double)g_config->l1_config.s);
    double l2_ht = 0;

    if(!g_config->l2_config.disabled) {
        l2_ht = L2_HIT_TIME_CONST +(L2_HIT_TIME_PER_S * (double)g_config->l2_config.s);
    }

    double words_per_block = (double)pow(2,g_config->l1_config.b)/ WORD_SIZE;
    double dram_penalty = DRAM_AT + (DRAM_AT_PER_WORD * words_per_block);

    stats->avg_access_time_l2 = l2_ht +(stats->read_miss_ratio_l2 * dram_penalty);
    stats->avg_access_time_l1 = l1_ht + (stats->miss_ratio_l1 * stats->avg_access_time_l2);
}
