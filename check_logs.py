#!/usr/bin/env python3
import os
import sys

def read_last_line(filepath):
	"""
	Read and return the last line of a file.
	"""
	if not os.path.exists(filepath):
		return "File does not exist"
		
	if os.path.getsize(filepath) == 0:
		return "File is empty"
		
	with open(filepath, 'r') as f:
		lines = f.readlines()
		
	if not lines:
		return "File has no content"
		
	return lines[-37].strip()


def process_log_files(log_dir="./logs"):
	"""
	Process all files in the logs directory and print the filename and last line.
	"""
	# Check if logs directory exists
	if not os.path.exists(log_dir):
		print(f"Error: Directory '{log_dir}' not found")
		return
	
	# Get all files in the logs directory
	files = [f for f in os.listdir(log_dir) if os.path.isfile(os.path.join(log_dir, f))]
	
	if not files:
		print(f"No files found in '{log_dir}' directory")
		return
		
	# Process each file
	for filename in sorted(files):
		filepath = os.path.join(log_dir, filename)
		last_line = read_last_line(filepath)
		print(f"File: {filename}")
		print(f"Last line: {last_line}")
		print("-" * 60)  # Separator for better readability

if __name__ == "__main__":
	# Allow for a custom logs directory if provided as an argument
	log_dir = sys.argv[1] if len(sys.argv) > 1 else "./logs"
	process_log_files(log_dir)
