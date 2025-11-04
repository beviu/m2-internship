# Tokio task experiments

How does the semantic gap between kernel and user level threads impact the
performance of Tokio? This is a series of microbenchmarks to evaluate that
performance impact.

## Experiment 1: Page faults with User-level Threads vs Kernel-level Threads

Each thread does a page fault. What difference does it make in the overall
execution time if we use user-level threads or kernel-level threads?

## Experiment 2: Counting how many times a Kernel-level Thread is scheduled out while running a User-level Thread

Would it be worth it to implement an optimization so that a runtime's kthread
can steal a task that was running on another kthread that was scheduled out? To
figure this out, we start by counting how many times a kthread is scheduled out
while it was running a uthread.
