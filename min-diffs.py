#!/usr/bin/env python3
import csv
import itertools
import sys

def array_differences(array):
    return list(map(lambda pair: pair[1] - pair[0], zip(itertools.chain([0], array), array)))

MIN_DURATIONS = None

with open(sys.argv[1]) as f:
    reader = csv.reader(f)
    COLUMNS = next(reader)
    for row in reader:
        values = [int(cell) for cell in row]
        durations = array_differences(values)
        if MIN_DURATIONS is None:
            MIN_DURATIONS = durations
        else:
            MIN_DURATIONS = list(map(lambda pair: min(*pair), zip(MIN_DURATIONS, durations)))

print(", ".join(COLUMNS))
print(", ".join(map(str, MIN_DURATIONS)))
