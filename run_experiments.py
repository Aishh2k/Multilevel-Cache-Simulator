import subprocess
import re
import csv
import os

C1_OPTS = [14, 15]  # 16KB - 32KB
C2_OPTS = [16, 17]  # 64KB - 128KB
B_OPTS = [5, 6, 7]   # 32B - 128B

S1_OPTS = [0, 1, 2, 3, 4] 
S2_OPTS = [1, 2, 3, 4, 8] 

L2_POLICIES = ["mip", "lip"] 
PREFETCHERS = ["none", "plus1", "markov", "hybrid"]
MARKOV_ROWS = [16, 32, 64, 128, 256, 512]
TRACES = ["gcc", "leela", "linpack", "matmul_naive", "matmul_tiled", "mcf"]

def calculate_metrics(c1, s1, c2, s2, b, prefetch, m_rows):
    data_b = (1 << c1) + (1 << c2)

    # L1 Metadata: Valid bit + dirty bit (+2)
    l1_blocks = 1 << (c1 - b)
    l1_tag_bits = 64 - (c1 - s1)
    l1_meta = l1_blocks * (l1_tag_bits + 2)
    
    # L2 Metadata: Valid bit + prefetch bit (+2)
    l2_blocks = 1 << (c2 - b)
    l2_tag_bits = 64 - (c2 - s2)
    l2_meta = l2_blocks * (l2_tag_bits + 2)
    
    # Total metadata in bits
    meta_b = (l1_meta + l2_meta)
    return data_b, meta_b

scoreboard = []

for trace in TRACES:
    out_file = f"results_{trace}.csv"
    best_aat = float('inf')
    min_meta = float('inf')
    best_config = None
    
    print(f"\nRUNNING EXPERIMENTS FOR: {trace}")
    
    with open(out_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["Trace", "C1", "B", "S1", "C2", "B", "S2", "L2 Policy", "Prefetch", "Rows", "AAT", "Data_B", "Meta_B"])

        for b in B_OPTS:
            for c1 in C1_OPTS:
                for c2 in C2_OPTS:
                    for s1 in S1_OPTS:
                        for s2 in [s for s in S2_OPTS if s >= s1]: 
                            for pol in L2_POLICIES:
                                for pref in PREFETCHERS:
                                    row_opts = MARKOV_ROWS if pref in ["markov", "hybrid"] else [0]
                                    for rows in row_opts:
                                        cmd = ["./cachesim", "-c", str(c1), "-b", str(b), "-s", str(s1), 
                                               "-C", str(c2), "-S", str(s2), "-P", pol, "-F", pref, "-r", str(rows)]
                                        
                                        try:
                                            with open(f"traces/{trace}.trace", "r") as t_file:
                                                proc = subprocess.run(cmd, stdin=t_file, capture_output=True, text=True)
                                            
                                            # Primary optimization metric: L1 AAT
                                            match = re.search(r"L1 average access time \(AAT\):\s+([\d.]+)", proc.stdout)
                                            if match:
                                                aat = float(match.group(1))
                                                data_b, meta_b = calculate_metrics(c1, s1, c2, s2, b, pref, rows)
                                                
                                                writer.writerow([trace, c1, b, s1, c2, b, s2, pol, pref, rows, aat, data_b, meta_b])
                                                
                                                # Identify optimal config with metadata tie-breaking
                                                if aat < best_aat or (abs(aat - best_aat) < 0.001 and meta_b < min_meta):
                                                    best_aat = aat
                                                    min_meta = meta_b
                                                    best_config = [trace, c1, b, s1, c2, b, s2, pol, pref, rows, aat, data_b, meta_b]
                                        except Exception: continue
    if best_config:
        scoreboard.append(best_config)

# Print rough summary for sxperiments seport
print("\n" + "-"*100)
print(f"{'Trace':<15} | {'C1':<2} | {'B':<2} | {'S1':<2} | {'C2':<2} | {'B':<2} | {'S2':<2} | {'POL':<4} | {'PREF':<8} | {'AAT':<7} | {'DATA_B':<8} | {'META_B':<8}")
print("-" * 100)
for e in scoreboard:
    print(f"{e[0]:<15} | {e[1]:<2} | {e[2]:<2} | {e[3]:<2} | {e[4]:<2} | {e[5]:<2} | {e[6]:<2} | {e[7]:<4} | {e[8]:<8} | {e[10]:<7.3f} | {e[11]:<8} | {e[12]:<8.0f}")