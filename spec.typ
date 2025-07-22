#import "@preview/bytefield:0.0.7": *
#import "@preview/subpar:0.2.2"
#import "./pf-privilege-flow.typ": pf-privilege-flow-diagram

#let paragraph-heading(body) = text(weight: "bold", body)

#let userfault-design = [
  We propose a modification to Intel CPUs to let applications handle Page Faults
  (PF) directly in user space.

  == Overview

  Currently, when an Intel CPU detects a page fault, it switches to privilege
  level 0 (this is the privilege level that corresponds to kernel-space) and jumps
  to the handler associated with page faults (`#PF`) in the interrupt descriptor
  table. The handler comes from the kernel and so the application cannot directly
  control page fault handling.

  _userfaultfd_ already lets application catch page faults. It works like this:

  + The application creates a userfaultfd File Descriptor (FD) with the
    `userfaultfd` system call.
  + The application initializes the userfaultfd FD with the `UFFDIO_API` ioctl,
    choosing between the _signal notification mode_ and the _FD notification
    mode_. We focus on the signal notitication mode here.
  + The application registers a range of memory for which page faults should be
    forwarded to user space using the `UFFDIO_REGISTER` ioctl.

  Then, whenever Linux catches a page fault, if the page fault is in a range of
  memory that was registered on a userfaultfd instance, the process receives a
  `SIGBUS` signal with the informations about the fault. Upon return from the
  signal handler, the kernel makes the application jump to the original faulting
  instruction.

  userfaultfd is used to implement _CRIU_ (Checkpoint/Restore In Userspace), a
  tool to freeze and checkpoint applications to the disk, and later resume them.
  It's also used to implement a similar checkpoint and restore functionality on
  QEMU (a virtual machine manager) when using KVM.

  userfaultfd was also used in _ExtMem_@jalalian24 to modify the policy used for
  virtual page allocations and page reclamation, as well as the swap backend. It
  showed that it could be possible to increase an application's performance by
  tuning the memory management policies and swap backend to the application.

  Following an existing trend of kernel bypass---the idea of having applications
  directly communicate with the hardware to improve performance---we propose a
  hardware modification to let applications directly handle page faults without
  going through the kernel first, as can be seen in @flow-of-privilege.

  #subpar.grid(
    columns: 2,
    column-gutter: 1.5cm,
    figure(pf-privilege-flow-diagram(false), caption: [Without UF]),
    figure(pf-privilege-flow-diagram(true), caption: [With UF]),

    caption: [The flow of privilege during the reception of a page fault in user
      space.],
    label: <flow-of-privilege>,
  )

  == Coexistence with Linux's page fault handler

  Applications might want to handle only a subset of the page faults and let Linux
  handle the rest as usual. For example, a database could let Linux handle page
  faults on the stack and executable mappings but handle page faults on the
  database file itself.

  The application might want to specify which page fault should be handled by the
  kernel versus by itself in the following ways:

  + a range of virtual addresses,
  + a `mmap` flag,
  + the entire heap,
  + the entire address space (except for the kernel part),
  + The application might ask for every page fault happening inside a code section
    to become a user fault.
  + A flag or attribute of a thread that indicates if every fault becomes a user
    fault.
  + A mix of the previous ideas.

  == Preliminary Specification

  #paragraph-heading[Hardware API.]

  ==== User fault handler

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

  ==== New instructions

  ===== `stuf`

  *Opcode*: `F3 0F 01 EB`

  Enables user faults.

  ===== `cluf`

  *Opcode*: `F3 0F 01 EA`

  Disables user faults.

  ===== `rdaddr`

  *Opcode*: `0F 01 D2`

  Reads the user fault virtual address into `rax`.

  ===== `kret`

  *Opcode*: `F3 0F 01 E9`

  #paragraph-heading[Software API]

  ```c
  int main() {
    uf_register_handler(handler);
    uf_enable();
    // ...
    uf_disable();
  }
  ```
]
