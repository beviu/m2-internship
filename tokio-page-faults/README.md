# Tokio task experiments

How does the semantic gap between kernel and user level threads impact the
performance of Tokio? This is a series of microbenchmarks to evaluate that
performance impact.

## Experiment 1: Page faults with User-level Threads vs Kernel-level Threads

Each thread does a page fault. What difference does it make in the overall
execution time if we use user-level threads or kernel-level threads?
