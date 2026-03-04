#!/bin/bash
set -e

student_stat_dir=student_outs
spotlight_benchmark=gcc
default_benchmarks=( gcc leela linpack matmul_naive matmul_tiled mcf )
plus1_benchmarks=( gcc leela matmul_tiled )
config_flags_l1='-D'
config_flags_l1_l2=
config_flags_l1_l2_plus1='-F plus1'
# Grad only:
markov_benchmarks=( gcc linpack matmul_naive mcf )
hybrid_benchmarks=( gcc leela matmul_naive matmul_tiled )
config_flags_l1_l2_markov='-F markov -r 100'
config_flags_l1_l2_hybrid='-F hybrid -r 100'
config_flags_l1_l2_markov50='-F markov -r 50'
config_flags_l1_l2_hybrid50='-F hybrid -r 50'

banner() {
    local message=$1
    printf '%s\n' "$message"
    yes = | head -n ${#message} | tr -d '\n'
    printf '\n'
}

student_stat_path() {
    local config=$1
    local benchmark=$2

    printf '%s' "${student_stat_dir}/${config}_${benchmark}.out"
}

ta_stat_path() {
    local config=$1
    local benchmark=$2

    printf '%s' "ref_outs/${config}_${benchmark}.out"
}

human_friendly_flags() {
    local config=$1

    local config_flags_var=config_flags_$config
    local flags="${!config_flags_var}"
    if [[ -n $flags ]]; then
        printf '%s' "$flags"
    else
        printf '(none)'
    fi
}

generate_stats() {
    local config=$1
    local benchmark=$2

    local config_flags_var=config_flags_$config
    ./run.sh ${!config_flags_var} <"traces/$benchmark.trace" >"$(student_stat_path "$config" "$benchmark")"
}

generate_stats_and_diff() {
    local config=$1
    local benchmark=$2

    printf '==> Running %s...\n' "$benchmark"
    generate_stats "$config" "$benchmark"
    if diff -u "$(ta_stat_path "$config" "$benchmark")" "$(student_stat_path "$config" "$benchmark")"; then
        printf 'Matched!\n\n'
    else
        printf '\nPlease examine the differences printed above. Benchmark: %s. Config name: %s. Flags to cachesim used: %s\n\n' "$benchmark" "$config" "$(human_friendly_flags "$config")"
    fi
}

main_undergrad() {
    mkdir -p "$student_stat_dir"

    banner "Testing only L1 cache..."
    generate_stats_and_diff l1 gcc

    banner "Testing default configuration (L1 and L2)..."
    for benchmark in "${default_benchmarks[@]}"; do
        generate_stats_and_diff l1_l2 "$benchmark"
    done

    banner "Testing +1 prefetcher configuration (L1, L2, and +1 Prefetcher)..."
    for benchmark in "${plus1_benchmarks[@]}"; do
        generate_stats_and_diff l1_l2_plus1 "$benchmark"
    done
}

main_grad() {
    main_undergrad
   
    banner "Testing Markov prefetcher configuration with 50 entries..."
    generate_stats_and_diff l1_l2_markov50 gcc

    banner "Testing Hybrid prefetcher configuration with 50 entries..."
    generate_stats_and_diff l1_l2_hybrid50 gcc

    banner "Testing Markov prefetcher configuration (L1, L2, and Markov Prefetcher)..."
    for benchmark in "${markov_benchmarks[@]}"; do
        generate_stats_and_diff l1_l2_markov "$benchmark"
    done

    banner "Testing Hybrid prefetcher configuration (L1, L2, and Hybrid Prefetcher)..."
    for benchmark in "${hybrid_benchmarks[@]}"; do
        generate_stats_and_diff l1_l2_hybrid "$benchmark"
    done
}

