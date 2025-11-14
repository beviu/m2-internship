#!/usr/bin/env python3
import pandas as pd
import sys

file = sys.argv[1]
df = pd.read_csv(file, sep=',')

print('Mean:')
print(df.mean())

print('\nStandard deviation:')
print(df.std())

print('\nQuartiles:')
print(df.quantile([0, .25, .5, .75, 1]))
