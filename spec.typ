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

+ A range of virtual addresses on which every fault will become a user fault.
+ A `mmap` flag that makes every fault on the memory mapping become a user fault.
+ The application might ask for every page fault on the heap to be a user fault.
+ The application might ask for every page fault the userspace part of the
  address space to be a user fault.
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

#bibliography("bibliography.yaml")
