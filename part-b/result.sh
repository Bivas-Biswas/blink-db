#!/bin/bash

# Define the parameters for concurrent requests and parallel connections
concurrent_requests=(10000 100000 1000000)
parallel_connections=(10 100 1000)

# Create result directory if not exists
mkdir -p result

# Run benchmarks
for req in "${concurrent_requests[@]}"; do
    for conn in "${parallel_connections[@]}"; do
        output_file="result/result_${req}_${conn}.txt"
        echo "Running benchmark: $req requests, $conn connections..."
        redis-benchmark -h 127.0.0.1 -p 9001 -t set,get,del -n "$req" -c "$conn" -e -r 1000 --csv > "$output_file"
    done
done

echo "All benchmarks completed. Results saved in result/ directory."
