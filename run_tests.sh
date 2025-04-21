#!/bin/bash

# Create logs directory if it doesn't exist
#rm -rf logs
#mkdir -p logs

# Read optimization level from stdin
echo "Enter optimization level (n):"
read opt_level

# Change to the directory containing the processor executable
cd mips_cpu

# Loop through all .o files in the test_data_pipeline/build/ directory
for file in ../test_data_pipeline/bins/*.o; do
    # Check if the file exists
    if [ -f "$file" ]; then
        # Extract the base filename without path and extension
        base_filename=$(basename "$file" .o)

        # Run the processor and output to log file
        ./processor --bmk="$file" -O${opt_level} > "../logs/${base_filename}-O${opt_level}.log" 2>&1
#        ./processor --bmk="$file" -O0 > "../logs/${base_filename}_o0.log" 2>&1

        # Check the exit status of the processor
        if [ $? -eq 0 ]; then
            echo "Processed $base_filename successfully"
        else
            echo "Error processing $base_filename" >&2
        fi
    fi
done

echo "Testing complete. Check the logs directory for results."
