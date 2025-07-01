#import "@preview/bytefield:0.0.7": *

= User Fault (UF)

We propose a modification to Intel CPUs to let applications handle page faults (PF)
directly in user space.

Currently, when an Intel CPU detects a page fault, it switches to privilege level 0 (the
privilege level of the OS) and jumps to the handler associated with page faults
(`#PF`) in the Interrupt Descriptor Table. The handler comes from the OS and so
the application cannot directly control page fault handling.

On Linux,  _userfaultfd_ already lets application catch page faults. It works
like this:

+ The application creates a userfaultfd file descriptor (FD) with the
  `userfaultfd` system call.
+ The application initializes the userfaultfd FD with the `UFFDIO_API` ioctl,
  choosing between the _signal notification mode_ and the _FD notification mode_.
  We will focus on the signal notitication mode here.
+ The application registers a range of memory for which page faults should be
  forwarded to user space using the `UFFDIO_REGISTER` ioctl.

Then, whenever Linux catches a page fault, if the page fault is in a range of
memory that was registered on a userfaultfd instance, the process receives a
`SIGBUS` signal with the informations about the fault. Upon return from the
signal handler, the kernel makes the application jump to the original faulting
instruction.

userfaultfd is used to implement _CRIU_ (Checkpoint/Restore In Userspace) which
is a tool to freeze and checkpoint applications to the disk, and later resume
them. It's also used to implement a similar checkpoint and restore functionality
on QEMU (a virtual machine manager) when using KVM.

userfaultfd was also used in _ExtMem_@extmem to modify the policy used for
virtual page allocations and page reclamation, as well as the swap backend. It
showed that it could be possible to increase an application's performance by
tuning the memory management policies and swap backend to the application.

The modification we propose provides a mechanism similar to userfaultfd, but
with better performance.

=== Coexistence with Linux's page fault handler

Applications might want to handle only a subset of the page faults and let
Linux handle the rest as usual. For example, a database could let Linux handle
page faults on the stack and executable mappings but handle page faults on the
database file itself.

The application might want to specify which page fault should be handled by the
kernel versus by itself in the following ways:

+ a range of virtual addresses,
+ a `mmap` flag,
+ the entire heap,
+ the entire address space (except for the kernel part),
+ The application might ask for every page fault happening inside a code
  section to become a user fault.
+ A flag or attribute of a thread that indicates if every fault becomes a user
  fault.
+ A mix of the previous ideas.

== Interface

```c
int main() {
  uf_register_handler(handler);
  uf_enable();
  // ...
  uf_disable();
}
```

= Implementation

- The address of the handler is stored in a MSR.
- `uf_enable`/`uf_disable` modify a flag in an MSR.

== User fault handler

The user fault handler has the same ABI as a user interrupt handler, except for
the vector number which is replaced by an _error code_ which encodes information
about the fault:

#bytefield(
  msb: right,
  bitheader("bounds"),
  flag("Pres."),
  flag("Write"),
  flag("User"),
  flag("Res."),
  flag("Fetch"),
)

== New instructions

=== `stuf`

*Opcode*: `F3 0F 01 EB`

Enables user faults.

=== `cluf`

*Opcode*: `F3 0F 01 EA`

Disables user faults.

=== `rdaddr`

*Opcode*: `0F 01 D2`

Reads the user fault virtual address into `rax`.

=== `kret`

*Opcode*: `F3 0F 01 E9`

= Questions

Should user interrupts and UF be able to coexist?

#pagebreak()

= Meeting notes

- Transfer control to kernel
- Transfer control to user (interface for the app to receive the kernel metadata from a VMA such as the PFNs so
  that the application can reuse them when enabling UFs)

Ring buffer for pending page faults:
- VA
- IP
- Type of page fault

Similar to Intel PML.

When the ring buffer is full, the sender generates a fault and traps in the kernel
so that it can schedule it out. The kernel notifies the handler that it needs to
handle a fault and tell the kernel when it's done so that it can schedule in the
sender again.

For the case where the handler is schedule out, we can use a mechanism similar to
uintr_wait, where the kernel changes the NV field in the UPID of the handler so
that trying to send a user interrupt to the handler thread will result in a trap
in the kernel.

We can reuse the UITT, as if it was a user interrupt. The receiver knows whether
a user interrupt comes from a user fault or a `senduipi` by looking at the vector
number.

We could have a different mechanism for when:
- the handler is the same thread as the faulting thread (use simpler mechanism
  with stack, separate from user interrupt, with separate handler)
- the handler is a different thread from the faulting thread (use user interrupt + ring buffer)

What about nesting? In the self-UF case, do we disable user faults before jumping
to the handler and renable them when we return from the handler? Do we do something
like user interrupt where the CPU pops RFLAGS during UIRET so software that wants
user interrupts disabled after UIRET has to modify the pushed RFLAGS.

We would have structure that points to the ring buffer and also contains metadata
such as the head and tail indices. They would both be shared between the faulting
and handling thread. They could be aligned to a cacheline.

Low-level interface:

Self UF:
- MSR with handler address.

Remote UF:
- ...

Actually, no need for remote UF. We can implement it in software.

Since we don't want to copy the user interrupt ABI, we can push instead of
`rdaddr`.

#bibliography("bibliography.yaml")
