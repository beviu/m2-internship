#import "@preview/cetz:0.3.3"
#import "@preview/drafting:0.2.2": *
#import "simple-arrow.typ": simple-arrow
#import "./execution.typ": *

#set heading(numbering: "1.1")

#set document(
  title: [M2 internship report],
  description: [Hardware-assisted user-space page fault handling],
  author: "Greg Depoire--Ferrer",
  date: datetime(year: 2025, month: 6, day: 1),
)

_M2 internship report_ #h(1fr) Greg #smallcaps[Depoire-\-Ferrer]

#v(1fr)

#let title(body) = align(center, block(width: 80%, align(center, [
  #line(length: 50%)
  #text(size: 20pt, weight: "bold", body)
  #line(length: 50%)
])))

#title[Hardware-assisted user-space page fault handling]

#pad(y: 1.5cm, align(center, block(width: 90%, grid(
  columns: (1fr, 1fr),
  column-gutter: 2em,
  align: horizon,
  [
    _Location_

    #v(.5em)

    #link("https://lig-krakos.imag.fr/")[KrakOS team], \
    #link(
      "https://www.liglab.fr/",
    )[Laboratoire d'Informatique de Grenoble~(LIG)]
  ],
  [
    _Supervisors_

    #v(.5em)

    Alain #smallcaps[Tchana] \
    #link("mailto:alain.tchana@grenoble-inp.fr")

    #v(.25em)

    Renaud #smallcaps[Lachaize] \
    #link("mailto:renaud.lachaize@univ-grenoble-alpes.fr")
  ],
))))

#let part(body) = block(above: 2.5em, below: 1.125em, text(
  size: 13.2pt,
  weight: "bold",
  body,
))

#part[Abstract]

This report proposes a hardware modification to Intel CPUs to optimize the
user-space page fault handling in Linux.

#v(1fr)

#h(1fr) June 26, 2025

#pagebreak()

#{
  show outline.entry.where(level: 1): set block(above: 1.2em)
  show outline.entry.where(level: 1): set text(weight: "bold")

  show outline.entry: set outline.entry(fill: pad(
    left: .5em,
    right: 1em,
    repeat([.], gap: 0.5em),
  ))
  show outline.entry.where(level: 1): set outline.entry(fill: none)

  outline()
}

= Introduction

== Page faults

_Virtual memory_ is a technique to control the memory visible and usable
by a program. With virtual memory, a program accesses memory using _virtual
addresses_ that the memory management unit (MMU) maps to _physical addresses_.

_Paging_ is an implementation of virtual memory that splits memory into _pages_.
The MMU converts a virtual address into a virtual page number and offset within
that page. Then it maps the virtual page number to a physical page number using
the _page table_. Finally, it converts the physical page number and offset
back into a physical address. In a system with pages of size $N$, virtual
address $v$ is mapped to physical address $p$ using the formula

$
  p = "table"(floor(v \/ N)) times N + (v "mod" N)
$

A _page table entry_ (PTE) can be _present_ or _missing_. A present PTE contains
a physical page number and access bits that specify if the virtual page is
readable and writable. A absent PTE contains arbitrary data ignored by the MMU.
When the MMU encounters a absent PTE or when the access bits deny the access
(for example, a write to a read-only virtual page), the processor generates an
exception called a _page fault_.

Paging can be used to make physical memory allocation lazy. On Linux, when a
program allocates virtual memory using the `mmap` system call#footnote[Without
  `MAP_POPULATE` and with memory overcommitment enabled (the default).], the
kernel creates a new _virtual memory area_ (VMA) but does not allocate physical
memory or modify the page table yet. Later, when the program accesses that memory
for the first time, the MMU sees that the PTE is absent, a page fault is
generated, and Linux takes over. Linux allocates a physical page, writes its
number into the PTE and resumes the execution of the program. In a similar
manner, paging is used to implement _swapping_ where a secondary storage (such
as disk) is used as additional memory when the operating system runs out of
physical memory.

== User-space delegation

On Linux, page faults are currently handled in kernel space. Moving the
handling of page faults to user space allows for more flexibility in the page
reclaim policy to use, as well as more complicated swap storage backends.

One implementation of user space page fault handling is by using the
`userfaultfd` functionality in newer Linux kernels@userfaultfd-doc. It allows
registering a region with a userfault file descriptor that will be readable
whenever a thread encounters a page fault in that region. Reading from this FD
will return a structure with information about the page fault and an `ioctl`
can be made on the FD to install a PTE and resume the execution of the faulted
thread.

= User interrupts

_User interrupts_ is a feature that allows delivering interrupts directly
to user space@intel-manual. It was first introduced in Intel Sapphire Rapids
processors.

== UPID

Threads that receive user interrupts need to have an _user posted-interrupt
descriptor_ or _UPID_ registered. The _UPID_ is managed by the OS. It is used
to by the `senduipi` instruction and during user-interrupt recognition and
delivery.

== UITT

Threads that send user interrupt need to have a _user-interrupt target table_ or
_UITT_ registered. This table contains _UITT entries_ that contain a pointer to
a UPID as well as a user-interrupt vector. The `senduipi` instruction takes an
index into this table.

= A model of the execution time of a minor page fault

To evaluate the hardware modifications that we propose, we build and validate
a model of the current page fault handling and then use it to approximate what
would be the execution time of a page fault with the modifications.

== Kernel page fault handling

We start by measuring the cost of a minor page fault on Linux without any
modification. A _minor page fault_ occurs when the physical page is directly
ready to be mapped by the operating system, as opposed to a _major page fault_
where there is work to do before the fault can be resolved (e.g. read a physical
page from disk). Minor page faults are the fastest.

One way to trigger a minor page fault is by reading from a page that was just
allocated. On Linux, newly allocated pages are filled with the zero byte. Linux
has an optimization where the the page fault from the first read to page is
treated by updating the PTE to point to a special physical page called the _zero
page_ that is filled with the zero byte and is shared among all processes. The
PTE is also updated to make the virtual page read-only so that when the page is
actually written to, a physical page is finally allocated.

#figure(caption: [Triggering a minor page fault])[
  ```c
  void *page = mmap(NULL, page_size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  /* Read the first byte of the page. */
  *(volatile char *)page;
  ```
]

The treatement of a page fault can be broken down into the following operations:

+ *Exception*: the processor detects the page fault and jumps to the operating
  system exception handler.
+ *Save state*: the operating system saves the registers of the program that
  generated the page fault.
+ *Read-lock VMA*: the operating system looks up the VMA containing the faulting
  virtual address, and locks it for reading.
+ *Handle fault*: the operating system fills in the physical page number in the
  PTE.
+ *Restore state*: the operating system restores the register to the ones that
  were saved before.
+ *IRET*: the operating system jumps back to the code that was executing before
  the page fault with the IRET (Interrupt RETurn) instruction.

The measurement methodology is described in @execution-time-measurement-method.
The results are shown in \@linux-page-fault-breakdown.

#inline-note[
  Rerun the experiments on the test machine and update the environment in the
  appendix.
]

#inline-note[
  Think about whether taking the minimum is a good idea. In any case, explain
  why I chose what I end up choosing. Don't forget to update the other table.
]

== `userfaultfd`-based page fault handling

Next, we measure the cost of a minor page fault handled by `userfaultfd` when
the handler immediately resolves the page fault with the zero page.

The `userfaultfd` thread continuously reads from the `userfaultfd` file
descriptor. When a page fault occurs, the read returns and the `userfaultfd`
thread does a `UFFDIO_ZEROPAGE` operation to resolve the page fault
with the zero page and wake up the faulting thread, as represented in
@userfaultfd-page-fault-flow.

#figure(
  execution(
    (
      [Faulting thread],
      user-stmt[Faulting instruction],
      internal-stmt[Exception],
      kernel-stmt[Save state],
      kernel-stmt[Read-lock VMA],
      kernel-stmt[Walk page table],
      kernel-stmt[Wake up \ `userfaultfd` thread],
      (set-event: "userfaultfd-readable"),
      (wait-for-event: "userfaultfd-handled"),
      kernel-stmt[Return from \ `handle_mm_fault`],
      (set-event: "treat-fault-end"),
      kernel-stmt[Read-lock VMA],
      kernel-stmt[Walk page table],
      kernel-stmt[Return from \ `handle_mm_fault`],
      kernel-stmt[Cleanup],
      kernel-stmt[Restore state],
      kernel-stmt[IRET],
    ),
    (
      [`userfaultfd` thread],
      (wait-for-event: "userfaultfd-readable"),
      kernel-stmt[Return from `read`],
      (set-event: "treat-fault-start"),
      user-stmt[SYSCALL],
      kernel-stmt[Save state],
      kernel-stmt[Read-lock VMA],
      kernel-stmt[Walk page table],
      kernel-stmt[Set PTE],
      kernel-stmt[Notify app thread],
      (set-event: "userfaultfd-handled"),
    ),
    braces: (
      (
        "treat-fault-start",
        "treat-fault-end",
        [Treat fault and \ return from \ `handle_mm_fault`],
      ),
    ),
  ),
  caption: [Flow of `userfaultfd`-based page fault handling],
) <userfaultfd-page-fault-flow>

The kernel read-locks the VMA and walks the page table three times. In between
the notification and the `UFFDIO_ZEROPAGE` operation, the VMA that contains
the faulting address could have been modified from another thread so the kernel
verifies that the VMA is still associated with the `userfaultfd` context, and
walks the page table a second time. I do not understand the third occurence,
however.

Measurements in \@userfaultfd-page-fault-breakdown show that `userfaultfd`-based
page fault handling is significantly slower than the native page fault handling.

// This is not a heading so that it does not appear in the outline.
#text(size: 15.4pt, weight: "bold", block(
  above: 1.29em,
  below: .54em,
  [Bibliography],
))

#bibliography("bibliography.yaml", title: none)

#let appendix(body) = {
  set heading(numbering: "A.1", supplement: [Appendix])
  counter(heading).update(0)
  body
}

#show: appendix

= Measuring execution time <execution-time-measurement-method>

#inline-note[Explain why kprobes and other alternatives were not considered.]

== RDTSC

The execution time of the operations we measure is in the order of a few
nanoseconds. We need a clock with a high frequency that is simple and fast to
read. We use the RDTSC (ReaD Time Stamp Counter) instruction. The _time stamp
counter_ is a counter driven by a clock whose frequency depends on the CPU but
is often in the order of the CPU's maximum frequency.

The RDTSC instruction is not serializing: the processor is free to reorder
previous and subsequent instructions to make full use of its instruction
pipeline. We use _fences_ to prevent instruction reordering around the RDTSC
instruction as suggested by the manual.

These fences slightly increase the execution time of the operations
that we measure but make the results easier to interpret. For example,
\@linux-page-fault-no-fence-breakdown shows the execution time of the same
operation as measured in \@linux-page-fault-breakdown, but without the fences.

== Minor page fault

Measuring in kernel space proved to be more difficult than measuring in user
space as we want to ignore all but one process, and there is no easy way to
store and/or communicate the measures back to user space.

To detect that the page fault comes from the process we want to measure, we
store a fixed value in a given register and check it on the other side.

#figure(caption: [Storing a magic value in a register])[
  ```asm
  /* Before the page fault. */
  movl $0xb141a52a, %rbx

  /* In the exception handler. */
  cmpl $0xb141a52a, %ebx
  ```
]

To communicate the measures to user space, we print them to the kernel buffer
using `printk` (similar to `printf` in userspace). The execution time of the
`printk` calls is large so we measure the TSC before and after the calls to
`printk` and subtract them.

#let cumulative-sum(array) = {
  let accumulator = array.at(0)
  let result = (accumulator,)
  for value in array.slice(1) {
    accumulator += value
    result.push(accumulator)
  }
  result
}

The code for measuring the execution time of the operations that happen during
a minor page fault can be found in the `page-fault-breakdown` directory of the
repository: there is a kernel patch as well as the source code of a program
to be run on a patched kernel that produces a CSV file where each row contains
the execution time of the operations for a page fault.

== Environment

The measurements were done in a virtual machine running Linux 6.14 on a _Intel
Core i5-1335U_ processor.
