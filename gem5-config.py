# Copyright (c) 2021-2025 The Regents of the University of California
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import os

from gem5.coherence_protocol import CoherenceProtocol
from gem5.components.boards.kernel_disk_workload import KernelDiskWorkload
from gem5.components.boards.x86_board import X86Board
from gem5.components.cachehierarchies.ruby.mesi_two_level_cache_hierarchy import (
    MESITwoLevelCacheHierarchy,
)
from gem5.components.memory.single_channel import SingleChannelDDR3_1600
from gem5.components.processors.cpu_types import CPUTypes
from gem5.components.processors.simple_switchable_processor import (
    SimpleSwitchableProcessor,
)
from gem5.isas import ISA
from gem5.resources.resource import BinaryResource, DiskImageResource
from gem5.simulate.exit_handler import ExitHandler
from gem5.simulate.simulator import Simulator
from gem5.utils.override import overrides
from gem5.utils.requires import requires

# This checks if the host system supports KVM. It also checks if the gem5
# binary is compiled to include the MESI_Two_Level cache coherence protocol.
requires(
    coherence_protocol_required=CoherenceProtocol.MESI_TWO_LEVEL,
    kvm_required=True,
)

parser = argparse.ArgumentParser(description="Run the Gem5 User Fault test image.")

# root partition is set to 1 by default.

parser.add_argument(
    "--image",
    type=str,
    required=False,
    default="@defaultDiskImage@",
    help="Path to the disk image.",
)

parser.add_argument(
    "--partition",
    type=str,
    required=False,
    default=None,
    help='Root partition of the disk image. If the disk is not partitioned, then pass "".',
)

parser.add_argument(
    "--kernel",
    type=str,
    required=False,
    default="@defaultKernel@",
    help="Path to the kernel.",
)

parser.add_argument("--action", type=str, required=True, help="The action to execute.")

args = parser.parse_args()

# Here we set up a MESI Two Level Cache Hierarchy.
cache_hierarchy = MESITwoLevelCacheHierarchy(
    l1d_size="16KiB",
    l1d_assoc=8,
    l1i_size="16KiB",
    l1i_assoc=8,
    l2_size="256KiB",
    l2_assoc=16,
    num_l2_banks=1,
)

# Set up the system memory.
memory = SingleChannelDDR3_1600(size="3GiB")

# Here we set up the processor. This is a special switchable processor in which
# a starting core type and a switch core type must be specified. Once a
# configuration is instantiated a user may call `processor.switch()` or
# `simulator.switch_processor()`, if using a hypercall exit handler, to switch
# from the starting core types to the switch core types. In this simulation
# we start with KVM cores to simulate the OS boot, then switch to the Timing
# cores for the command we wish to run after boot.

processor = SimpleSwitchableProcessor(
    starting_core_type=CPUTypes.KVM,
    switch_core_type=CPUTypes.TIMING,
    isa=ISA.X86,
    num_cores=2,
)


# Our kernel assigns the IDE drive to /dev/sda instead of /dev/hda which is what
# X86Board assumes.
class SdaX86Board(X86Board):
    @overrides(KernelDiskWorkload)
    def get_disk_device(self):
        return "/dev/sda"


# Here we set up the board. The X86Board allows for FS mode (full system) or
# SE mode (syscall emulation) X86 simulations.


board = SdaX86Board(
    clk_freq="3GHz",
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
)

image = args.image

# We expect the user to input the full path of the disk-image.
if image[0] != "/":
    # We need to get the absolute path to this file. We assume that the file is
    # present on the current working directory.
    image = os.path.abspath(image)

board.set_kernel_disk_workload(
    kernel=BinaryResource(local_path=args.kernel),
    disk_image=DiskImageResource(image, root_partition=args.partition),
    readfile_contents=args.action,
)


class SwitchProcessorExitHandler(ExitHandler, hypercall_num=8):
    @overrides(ExitHandler)
    def _process(self, simulator: "Simulator") -> None:
        simulator.switch_processor()

    @overrides(ExitHandler)
    def _exit_simulation(self) -> bool:
        return False


simulator = Simulator(board=board)
simulator.run()
