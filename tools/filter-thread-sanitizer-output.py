"""
You can pipe clang-thread-sanitizer output to this program, and it'll filter it, removig false positives indicated in the blacklist file below
You run neblio-qt with, as an example:
neblio-qt --noquicksync 2<&1 | python tools/filter-thread-sanitizer-output.py | tee output.txt
"""

import sys
import os
import fnmatch
import copy


separator = '=================='  # separator between different messages from sanitizer

sanitizer_blacklist_file = "../sanitizer-blacklist.txt"

############################################################

import datetime
print(datetime.datetime.utcnow())

sanitizer_blacklist_file_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), sanitizer_blacklist_file)

with open(sanitizer_blacklist_file_path, 'r') as content_file:
    blacklist_data = content_file.read()

blacklisted_expressions = []

def parse_blacklist_data(data):
    lines = blacklist_data.replace("\r", "").split("\n")
    for line in lines:
        line = line.split("#")[0] # remove comments
        if len(line.replace(" ", "")) == 0:
            continue
        split_line = line.split(":")
        if len(split_line) != 2:
            raise ValueError("Lines in the blacklist file are expected to have two entries separated by ':'; this line is invalid: " + line)
        blacklisted_expressions.append(split_line[1])

parse_blacklist_data(blacklist_data)

print("Filtering for: " + str(blacklisted_expressions))

READSTATE_in_section = 0
READSTATE_out_of_section = 1

state = READSTATE_out_of_section

# data queue is the data that's received from stdin
data_queue = []

def process_data_queue(data_queue):
    prev_len = len(data_queue)
    orig_data_queue = copy.deepcopy(data_queue)  # since filtering will ruin the list
#    print("analyzing...")
    matched = False
    for blacklist_line in blacklisted_expressions:
        data_queue = copy.deepcopy(orig_data_queue)
        data_queue = fnmatch.filter(data_queue, blacklist_line)
        if len(data_queue) > 0:  # filtering will keep anything that matches the pattern provided
            print("Skipping on match of '" + blacklist_line + "' in: " + str(orig_data_queue) + "\n")
            matched = True
            break            
    # check the number of stack trace lines
    if not matched:
        found_stack_trace_line_with_high_id = False
        for queue_line in orig_data_queue:
            if queue_line.strip().startswith("#2"):
                found_stack_trace_line_with_high_id = True
                break
        if not found_stack_trace_line_with_high_id:
            print("Skipping on low number of stack trace lines for: " + str(orig_data_queue) + "\n")
            matched = True

    if not matched:  # if the filter doesn't match anything, then this is not filtered, print it
        sys.stdout.write(separator + "\n")
        sys.stdout.write("\n".join(orig_data_queue))
        sys.stdout.write(separator + "\n")

    sys.stdout.flush()
    data_queue = []

for line in sys.stdin:
    if line.startswith(separator) and state == READSTATE_out_of_section:
#        print("out")
        state = READSTATE_in_section

    elif line.startswith(separator) and state == READSTATE_in_section:
#        print("in-separator")
        state = READSTATE_out_of_section
        process_data_queue(data_queue)
        data_queue = []

    elif not line.startswith(separator) and state == READSTATE_in_section:
#        print("in-section")
        data_queue.append(line)

if len(data_queue) > 0:  # process anything left
    process_data_queue(data_queue)
