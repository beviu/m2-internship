#import "@preview/cetz:0.3.3"
#import "@preview/drafting:0.2.2": *
#import "simple-arrow.typ": simple-arrow
#import "./execution.typ": *

#set document(
  title: [M2 internship report],
  description: [Hardware-assisted user-space page fault handling],
  author: "Greg Depoire--Ferrer",
  date: datetime(year: 2025, month: 7, day: 1),
)

#set page(numbering: "1")

#set heading(numbering: "1.1")

#let link-color = olive.darken(20%)

#show link: set text(link-color)
#show ref: set text(link-color)

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

#h(1fr) July 1, 2025

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

In the _Memory Management_ (MM) part of operating systems, the smallest unit of
memory is called a _page_. A page is a contiguous and aligned block of
memory---often 4 KiB.

A _Page Fault_ (PF) occurs when a process accesses a memory address for which
the corresponding page is either not present or the access violates permissions.
Page faults are caught and handled by the operating system. The role of the
operating system is to either terminate the process if the access is deemed
invalid, or to resolve the page fault---often by allocating a page for the
process---and to restart the process at the instruction that caused the page
fault.

Allocating pages is an operation that requires special permissions which
applications do not have. Applications run in _user space_ as opposed to the
operating system which runs in _kernel space_. When the processor detects a page
fault, it switches to kernel space and gives control to the operating system.

The goal of this internship is to optimize the process by which certain page
faults are handled not by the operating system, but by user-space applications.

== Why delegate page faults to user space?

Delegating page fault handling to user space enables the live migration of
virtual machines and container and the optimization of workload-specific memory
management.

=== Live migration

Cloud providers are incentivized to reduce their downtime as much as possible,
but servers frequently need maintenance. Live migration allows them to relocate
the clients' virtual machines and containers to a different server without
stopping them before the maintenance is started.

Additionally, to make better use of their hardware, cloud providers often
overallocate resources. When the load on a single server exceeds the server's
capacity, they can move virtual machines and containers off of it with no
downtime using live migration.

Live migration of virtual machines and processes is implemented by lazily
copying the memory from a server to another by hooking into page fault handling
@live-migration-of-vms@checkpoint-and-restart. Unfortunately, popular operating
system are built to be general purpose and often don't implement live migration.
Therefore in practice, live migration is implemented in user space by having the
operating system delegate the handling of some page faults to the migration
application (running in user space).

=== Optimizing workload-specific memory management

Memory is a critical resource in data centers so the memory management logic of
an operating system has impact on performance, memory waste and high carbon
emisison. Linux has a general-purpose memory manager which can be suboptimal for
some workloads so some papers have explored optimizing it---often to the benefit
of some workloads but the detriment of others.

#inline-note[Which papers?]

During my L3 internship (also in the LIG laboratory), I had already worked with
Papa Assane Fall, a PhD student working on USM which makes the Linux memory
manager more flexible by delegating parts of it to user space with the intent of
making it easier to tune it to specific workloads. ExtMem@extmem and FBMM@fbmm
are different implementation of this idea. Compared to USM, ExtMem uses an
existing but less efficient mechanism in Linux called userfaultfd@userfaultfd to
catch page faults, and FBMM proposes to expose the extensibility of the Linux
memory manager through the _Virtual FileSystem_ (VFS) interface.

#inline-note[Add LKML discussion about introducing eBPF in the MM.]

Delegating page faults to user space and implementing the better
workload-specific policies in user space has the advantage of being easier for
policy developers and system administrators because user-space code is easier to
modify, test and deploy than kernel code.

#inline-note[Explain this better.]

== Current inefficiencies in the delegation of page faults to user space

Userfaultfd is one of the most common mechanisms in Linux to catch page faults
in user space, but due to its implementation, it incurs large latency costs
which we will measure later. #margin-note[Link to the section that shows this.]
USM improves on userfaultfd but in order to co-exist with the Linux memory
manager, it still has to wait for the operating system to decide if the page
fault should be handled by USM or not which also incurs latency.

In addition to these costs, because current processors assume that it is the
role of the operating system to handle page faults, detours have to be taken to
delegate page faults to user space which also incurs latency.

In this internship, we propose a hardware modification of the processor that
would reduce the latency of receiving a page fault in user space.

= Background

== Page faults

_Virtual memory_ is a technique to control the memory visible and usable by a
program. With virtual memory, a program accesses memory using _virtual
addresses_ that the _Memory Management Unit_ (MMU) maps to _physical addresses_.

_Paging_ is an implementation of virtual memory that splits memory into _pages_,
with a _page table_ that maps virtual pages to physical pages. It is the most
common implementation of virtual memory and the one that we focus on in this
report.

When the processor access memory and paging is enabled, the MMU converts the
virtual address into a virtual page number and offset within that page. Then it
reads the _Page Table Entry_ (PTE) associated with the virtual page number to
find the associated _Physical Page Number_ (PPN). Finally, it converts the
physical page number and offset back into a physical address. In a system with
pages of size $N$, virtual address $v$ is mapped to physical address $p$ using
the formula

$
  p = "pfn_of_pte"(floor(v \/ N)) times N + (v "mod" N)
$

A PTE can be _present_ or _missing_. A present PTE contains a physical page
number and access bits that specify if the virtual page is readable and
writable. A absent PTE contains arbitrary data ignored by the MMU.

When the MMU encounters a absent PTE or when the access bits deny the access
(for example, a write to a read-only virtual page), the processor generates an
exception called a _page fault_. Page faults can be used to make physical memory
allocation lazy, as we will see next with the case of Linux.

== Basics of Linux memory management

=== Page fault handling

On Linux, when a program allocates virtual memory using the `mmap` system
call#footnote[Without `MAP_POPULATE` and with memory overcommitment enabled (the
  default).], the kernel creates a new _Virtual Memory Area_ (VMA) but does not
allocate physical memory or modify the page table yet. Later, when the program
writes to that memory for the first time, the MMU sees that the PTE is absent, a
page fault is generated, and Linux takes over. Linux allocates a physical page,
writes its number into the PTE and resumes the execution of the program. In a
similar manner, paging is used to implement _swapping_ where secondary
storage---storage that is slower than RAM (e.g. disk)---is used as additional
memory when the operating system runs out of physical memory.

#inline-note[Develop section.]

=== User-space memory manager

Page fault handling can be delegated to user space using the `userfaultfd`
functionality in newer Linux kernels@userfaultfd. It allows registering a region
with a _userfault File Descriptor_ (FD) that will be readable whenever a thread
encounters a page fault in that region. Reading from this FD will return a
structure with information about the page fault and an `ioctl` can be made on
the FD to install a PTE and resume the execution of the faulted thread.

#inline-note[Explain how ExtMem and USM work.]

== User Interrupt

_User Interrupt_ (UINTR) is a feature that allows delivering interrupts directly
to user space@intel-manual. It was first introduced in Intel Sapphire Rapids
processors.

Threads that receive user interrupts need to have an _User Posted-Interrupt
Descriptor_ (UPID) registered. The UPID is managed by the OS. It is used to by
the `senduipi` instruction and during user-interrupt recognition and delivery.

#inline-note[Develop section.]

= Benchmark

(One of the PhD students was working on publishing a paper for USM and I wanted
to help him to get finer-grained benchmarks)

== Methodology

I analyzed the page fault code in Linux (and in the various papers) which gave
me the impression that some could be (1) avoided or (2) optimized.

=== Part identification

+ I thought that we could remove the ISR entry part and ISR exit part by
  skipping the kernel.
+ Skip IRET.
+ Time from ISR exit back to user space, and time from user space to ISR entry.
+ When a page fault occurs, Linux finds the VMA associated with the virtual
  address. This takes time. Additionally, Linux needs to acquire locks to
  prevent concurrent access to the VMA list structure.
+ USM changes the page allocation policy so I thought it would be interesting to
  time page allocation in the papers/variants. It was also useful for the USM
  paper.
+ I also wanted to measure the USM communication paths because I didn't
  understand why USM would be faster.

==

Validating the initial hypothesis: we gain performance by doing a hardware
modification

= Hardware modification proposal

There is an existing trend of kernel bypass (examples of papers) which served us
as a source of inspiration.

Kernel bypass needs special hardware support, so we sought to propose new
hardware modifications.

#pagebreak()


Threads that send user interrupt need to have a _user-interrupt target table_ or
_UITT_ registered. This table contains _UITT entries_ that contain a pointer to
a UPID as well as a user-interrupt vector. The `senduipi` instruction takes an
index into this table.

= A model of the execution time of a minor page fault

To evaluate the hardware modifications that we propose, we build and validate a
model of the current page fault handling and then use it to approximate what
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
thread does a `UFFDIO_ZEROPAGE` operation to resolve the page fault with the
zero page and wake up the faulting thread, as represented in
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
the notification and the `UFFDIO_ZEROPAGE` operation, the VMA that contains the
faulting address could have been modified from another thread so the kernel
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

These fences slightly increase the execution time of the operations that we
measure but make the results easier to interpret. For example,
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

The code for measuring the execution time of the operations that happen during a
minor page fault can be found in the `page-fault-breakdown` directory of the
repository: there is a kernel patch as well as the source code of a program to
be run on a patched kernel that produces a CSV file where each row contains the
execution time of the operations for a page fault.

== Environment

The measurements were done in a virtual machine running Linux 6.14 on a _Intel
Core i5-1335U_ processor.

= Notes

Do not talk about the process.

II. Background:
+ Allocateur mémoire de Linux (Papier USM)
  + Lazy
  + Buddy alloc
+ User-space memory alloc (principe) (papier USM)
  + Pourquoi ? Besoin de custom.
  + Architecture générale : expliquer les différentes use cases (proc à proc,
    etc.)
+ UINTR (papier USM et Yves)
  + UINTR is being studied in the research community in terms of its
    opportunities: xUI Gaetan

III. Motivation: PF forwarding to user space has high latency. and related work
+ Problem-statement: Voyage d'un page fault jusqu'à user space (avec nuances
  KPTI, ...) (UFFD et USM) (le chemin allée, pas le chemin réponse)
+ Problem-assessment: quantifier (UFFD et USM): comparer avec PF traité dans le
  noyau (fautes mineures)

IV. User Fault (UF) Design

+ Overview: Fournir une modification matérielle et une librairie à utiliser dans
tous les uses cases présentés en haut de façon sync et async pour pouvoir
traiter les défauts de pages en userspace avec fallback potentiel en kernel
space et de façon très flexible (ce sont les requirements)

(pour la flexibilité, parler par exemple des uses cases dans la spec)

+ Preliminary specification Hardware API (Software API imaginée)

  -> Mettre des exemples de code tant que possible (pas obligé de mettre pour
  chaque use case

+ Preliminary prototype La spec est le fruit de notre brainstorm mais
  l'implémentatino c'est lui qui l'a faite
