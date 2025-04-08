#!/bin/bash
sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'

# Start process with output redirection
./speed > speed.log 2>&1 &
pid=$!

# Track memory usage in background
( while ps -p $pid >/dev/null; do
    pmap $pid | grep total >> memory.log
    sleep 0.01
done ) &

wait $pid  # Wait for process exit

# Get last memory measurement
if [ -s memory.log ]; then
    echo "Peak memory usage:"
    tail -n 1 memory.log
else
    echo "Process exited before first measurement"
fi