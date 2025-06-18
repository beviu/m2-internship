#!/bin/bash

tmux split-window "sudo ./project-2 examples/project-2/cfg/alloc_policy_assignment_strategy.cfg"
sleep 5
tmux split-window "sudo taskset -c 0 ./usmWaker"
