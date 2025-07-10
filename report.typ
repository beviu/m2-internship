#import "@preview/bytefield:0.0.7": *
#import "@preview/cetz:0.4.0"
#import "@preview/cetz-plot:0.1.2"
#import "@preview/drafting:0.2.2": *
#import "@preview/subpar:0.2.2"
#import "./execution.typ": execution, internal-stmt, kernel-stmt, user-stmt

#let paragraph-heading(body) = text(weight: "bold", body)

#set document(
  title: [M2 Internship Report],
  description: [Hardware-Assisted User-Space Page Fault Handling],
  author: "Greg Depoire--Ferrer",
  date: datetime(year: 2025, month: 7, day: 7),
)

#set page(numbering: "1")

#set heading(numbering: "1.1")

_M2 Internship Report_ #h(1fr) Greg #smallcaps[Depoire-\-Ferrer]

#v(1fr)

#let title(body) = align(center, block(width: 80%, align(center, [
  #line(length: 50%)
  #text(size: 20pt, weight: "bold", body)
  #line(length: 50%)
])))

#title[Hardware-Assisted User-Space Page Fault Handling]

#pad(y: 1.5cm, align(center, block(width: 90%, grid(
  columns: (1fr, 1fr),
  column-gutter: 2em,
  row-gutter: 1.7em,
  align: horizon,
  text(style: "italic")[Location], text(style: "italic")[Supervisors],
  [
    #link("https://lig-krakos.imag.fr/")[KrakOS team], \
    #link(
      "https://www.liglab.fr/",
    )[Laboratoire d'Informatique de Grenoble~(LIG)]
  ],
  [
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
reception of page faults in user space.

// COMMENT-RENAUD: Il me semble que le sujet est plus large et qu'une première étape de la contribution inclut aussi des optimisations purement logicielles, non ?

#v(1fr)

#h(1fr) July 7, 2025

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

#pagebreak()

= Introduction

In the _Memory Management_ (MM) part of operating systems (OSes), the smallest
unit of memory is called a _page_. A page is a contiguous and aligned block of
memory---often 4 KiB.

A _Page Fault_ (PF) occurs when a process accesses a memory address for which
the corresponding page is either not present or the access violates permissions.
The CPU hands control over to the OS kernel. The kernel either terminates the
process if the access is deemed invalid, or resolves the fault---often by
allocating a page for the process---and restarts the process at the instruction
that caused the fault.

Applications run in _user space_ as opposed to the OS kernel which runs in
_kernel space_---a special mode of execution with total control over the
machine. In particular, this is what allows the kernel to allocate a page for
the process.

// COMMENT-RENAUD: Je pense qu'il faut ici commencer par dire qu'il est également possible de traiter des page faults en user space. Mais que ces techniques ont des performances parfois limitées. Et que le sujet du stage consiste à proposer des optimisations pour cela.

On Linux, _userfaultfd_ lets applications handle page faults in user space.
Unfortunately, userfaultfd increases the latency of page faults. The goal of
this internship is thus to optimize the process by which some page faults are
handled not by the kernel, but by (user-space) applications.

= Background

== Page Faults

_Virtual memory_ is the concept of giving processes a virtual view of the
machine's memory. Processes access memory through _virtual addresses_ which are
transparently mapped to _physical addresses_ (addresses into the actual RAM) by
the _Memory Management Unit_ (MMU) in the CPU. The mapping is set up by the
kernel. This lets the kernel control the memory visible to a process and make it
look like there is more memory than there actually is in the machine.

_Paging_ is how virtual memory is implemented in practice#footnote[Virtual
  memory can also be implemented with segmentation, but this is very rare.].
Memory is split into _pages_ and somewhere in the physical memory is a _page
table_ that maps virtual pages to physical pages.

#figure(
  {
    let page(label, free: true) = {
      let fill = if free { none } else { red.transparentize(80%) }

      grid.cell(
        inset: .75em,
        fill: fill,
        label,
      )
    }

    let pte(label, absent: true) = {
      let fill = if absent { none } else { red.transparentize(80%) }

      grid.cell(
        inset: (x: .75em, y: .5em),
        fill: fill,
        label,
      )
    }

    let dots = grid.cell(
      inset: (x: .75em, y: .7cm),
      math.dots.v,
    )

    // I don't know of an easy way to draw arrows between elements, so let's put the content in a CeTZ canvas and draw the arrows using absolute positions.
    cetz.canvas({
      import cetz.draw: content, line

      content((), anchor: "north-west", grid(
        columns: (3.5cm,) * 3,
        row-gutter: .75em,
        align: (row, column) => if row == 0 {
          center + bottom
        } else {
          center + top
        },
        [Physical Memory (entire machine)],
        [Page Table (per~process)],
        [Virtual Memory (per~process)],

        grid(
          stroke: 1pt,
          dots,
          ..range(3, -1, step: -1).map(i => {
            page([Physical Page #i], free: i >= 3)
          }),
        ),
        grid(
          stroke: 1pt,
          inset: .5em,
          dots,
          ..range(3, -1, step: -1).map(i => {
            let label = if i == 1 { [\#0] } else { [Absent] }
            pte(label, absent: i != 1)
          }),
        ),
        grid(
          stroke: 1pt,
          dots,
          ..range(3, -1, step: -1).map(i => {
            page([Virtual Page #i], free: i != 1)
          }),
        ),
      ))

      line((7.35, -4.8), (6.1, -4.35), mark: (end: ">"))
      line((4.4, -4.35), (3.28, -5.7), mark: (end: ">"))
    })
  },
  caption: [With paging, virtual pages are mapped to physical pages through the
    page table.],
)

When the CPU accesses memory and paging is enabled, the MMU converts the virtual
address into a virtual page number and offset within that page. Then it reads
the _Page Table Entry_ (PTE) associated with the virtual page number to find the
associated _Physical Page Number_ (PPN). Finally, it converts the physical page
number and offset back into a physical address. In a system with pages of size
$N$, virtual address $v$ is mapped to physical address $p$ using the formula

$
  p = "ppn_of_pte"(floor(v \/ N)) times N + (v "mod" N)
$

A PTE can be _present_ or _missing_. A present PTE contains a physical page
number and access bits that specify if the virtual page is readable and
writable. A absent PTE contains arbitrary data ignored by the MMU.

When the MMU encounters a absent PTE or when the access bits deny the access
(for example, a write to a read-only virtual page), the CPU generates an
exception called a _page fault_. Page faults can be used to make physical memory
allocation lazy, as we will see next with the case of Linux.

== Memory Allocation on Linux

#paragraph-heading[Virtual Memory Allocation.] On Linux, virtual memory is
_lazily-allocated_: when a program allocates virtual memory using the `mmap`
system call#footnote[Without `MAP_POPULATE` and with memory overcommitment
  enabled (the default).], the kernel does not allocate physical memory or
modify the page table yet.

The kernel creates a new _Virtual Memory Area_ (VMA) to record the allocation
and it is only when the program accesses a page of that memory area for the
first time that the MMU sees that the PTE is absent, a page fault is generated,
and Linux takes over to find a suitable physical page. Linux writes the Physical
Page Number (PPN) into the PTE and resumes the execution of the program.

// COMMENT-RENAUD: Dans le paragraphe précédent, je suggère de dire: "... when the program accesses a page of that memory area for the first time ..."

Linux also uses paging to implement _swapping_ which allows using secondary
storage---storage that is slower than RAM (e.g., disk)---as additional memory
when the OS runs out of physical memory.

#paragraph-heading[The Buddy Allocator.] In order to allocate physical pages,
Linux uses a _buddy allocator_. Memory is split into blocks of pages. Each block
contains $2^i$ pages, where $i$ is called the _order_ of the block. Initially,
there is one free block that spans the entirety of the memory. When an
allocation request comes in with order $i$, the allocator checks if there is a
free block of order $i$. If there is, then it is returned. Otherwise, the
allocator looks for a larger free block and splits it recursively until the
block is of size $i$. When a block is freed, the allocator tries to merge it
with adjacent free blocks recursively.

// #inline-note[This is my understanding of buddy allocation based on reading the Wikipedia page. Is is how the buddy allocator works in Linux? COMMENT-RENAUD: Oui je pense que cela résume convenablement l'idée générale.]

== Externalization of Memory Allocation to User Space

The memory manager of an OS has an impact on performance and memory waste. Linux
has a general-purpose memory manager which can be suboptimal for some workloads
so there is ongoing research in optimizing it but the proposed changes are often
not integrated into Linux because they are to the benefit of some workloads but
the detriment of others.

// COMMENT-RENAUD : Le fait de mentioner "high-carbon emissions" ici (sans aucun contexte et spécifiquement pour le MM) est peut-être un peu déconcertant.

For example, FBMM@fbmm is a modification to Linux that makes it easier for
developers to modify the memory management by writing kernel modules. However,
kernel-space code such as kernel modules are harder to modify, test and deploy
than user-space code. This is why USM uses another approach by delegating parts
of the Linux memory manager to user-space. ExtMem@extmem is another
implementation of this idea which uses an existing but less efficient mechanism
in Linux called userfaultfd@userfaultfd to catch page faults.

// COMMENT-RENAUD: Un peu surprenant de finir par FBMM qui n'est pas en user space.
// Peut-être inverser l'ordre. Commencer par parler du besoin d'extensibilité/customisation.
// Mentionner FBMM comme un exemple de MM extensible en kernel space puis parler des frameworks de MM extensibles en user space.

// #inline-note[Maybe add LKML discussion about introducing eBPF in the MM?]

#paragraph-heading[General Architecture.] In ExtMem, the faulting thread handles
its own fault. In USM, a separate process handles faults. @levels-of-isolation
presents the types of isolation that we focus on.

#{
  let thread(
    start,
    label: none,
    wave-count: 1,
    wave-length: 1,
    wave-distance: .3,
    name: none,
  ) = {
    import cetz.draw: anchor, content, group, line

    group(name: name, {
      if label != none {
        content(
          start,
          anchor: "north",
          label,
          name: "label",
        )

        anchor("start", (rel: (0, -.5em), to: "label.south"))
      } else {
        anchor(
          "start",
          start,
        )
      }

      for i in range(wave-count) {
        let x = (i - (wave-count - 1) / 2) * wave-distance
        cetz.decorations.wave(
          line(
            (rel: (x, 0), to: "start"),
            (rel: (x, -wave-length), to: "start"),
          ),
          amplitude: .1,
          segments: 3,
        )
      }
    })
  }

  let process(
    a,
    b,
    threads: (),
    label: none,
    thread-distance: .8,
    wave-length: 1,
    wave-distance: .3,
    anchor: none,
    name: none,
  ) = {
    import cetz.draw: content, group, line, mark, rect

    group(name: name, {
      rect(
        a,
        b,
        anchor: anchor,
        name: "box",
      )

      content(
        (rel: (.5em, -.5em), to: "box.north-west"),
        anchor: "north-west",
        label,
      )

      for (i, style) in threads.enumerate() {
        let x = (i - (threads.len() - 1) / 2) * thread-distance
        thread(
          (rel: (x, 0), to: "box.center"),
          label: style.at("label", default: none),
          wave-count: style.at("wave-count", default: 1),
          wave-length: wave-length,
          wave-distance: wave-distance,
        )
      }
    })
  }

  subpar.grid(
    columns: 3,
    align: top,
    figure(
      cetz.canvas({
        process(
          (0, 0),
          (2, 2),
          label: $P_1$,
          threads: (
            (wave-count: 2, label: $T_1$),
          ),
        )
      }),
      caption: [The faulting thread handles the fault.],
    ),
    figure(
      cetz.canvas({
        process(
          (0, 0),
          (2, 2),
          label: $P_1$,
          threads: (
            (wave-count: 1, label: $T_1$),
            (wave-count: 1, label: $T_2$),
          ),
        )
      }),
      caption: [The handler runs in the same process as the faulting code, but
        in a separate thread.],
    ),
    figure(
      cetz.canvas({
        process(
          (0, 0),
          (2, 2),
          label: $P_1$,
          threads: (
            (wave-count: 1, label: $T_1$),
          ),
        )
        process(
          (3, 0),
          (5, 2),
          label: $P_2$,
          threads: (
            (wave-count: 1, label: $T_2$),
          ),
        )
      }),
      caption: [The handler is moved to separate process.],
    ),
    caption: [Various levels of isolation between the faulting code and the
      handler],
    label: <levels-of-isolation>,
  )
}

In addition to different types of isolation, we consider two modes of execution:
_synchronous_ and _asynchronous_.

// COMMENT-RENAUD: Je pense qu'il faut dire plus clairement que les optimisations que tu proposes (dans la suite du rapport) ont vocation en prendre en compte ces différents cas de figure.

#inline-note[How to explain this?]

// COMMENT-RENAUD : Peut-être (à confirmer par Alain) en disant que synchrone correspond au cas au le thread qui déclenche la faute ne peut pas être réutilisé pour autre chose en attendant la résolution de la faute.

== UINTR

#inline-note[The two paragraphs below are pretty much copied verbatim from
  Yves's paper.]

_User Interrupt_ (UINTR) is a recent hardware feature of Intel's Xeon CPUs that
allows a sender thread to send an interrupt to a CPU, triggering the execution
of a routine previously set up by the receiver thread (within the same process
or a different process), without any intervention of the OS kernel. It is new
Inter-Process/Thread Communication (IPC) mechanism with lower latency than OS
signals@kone25@intel-manual.

#paragraph-heading[Hardware Interface.] In a nutshell, UINTR relies on two main
types of (per-thread) data structures allocated and initialized by the OS---User
Posted-Interrupt Descriptor (UPID) and User-Interrupt Target Table (UITT)---as
well as several privileged CPU registers, and the Local Advanced Programmable
Interrupt Controller (LAPIC) of each CPU core.

#figure(
  cetz.canvas({
    import cetz.draw: bezier, content, line

    content(
      (),
      [`senduipi(n)`],
      name: "senduipi",
    )

    content(
      (rel: (1.5, 0), to: "senduipi.east"),
      anchor: "west",
      table(
        columns: 2,
        stroke: 1pt,
        inset: .5em,
        align: center,
        table.cell(colspan: 2, math.dots.v),
        [`@UPID`], [$v$],
        table.cell(colspan: 2, math.dots.v),
      ),
      name: "uitt",
    )

    content((rel: (0, .4em), to: "uitt.north-west"), anchor: "south-west", [UITT
      (sender thread)])

    content(
      (rel: (3, 1.3), to: "uitt.east"),
      anchor: "west",
      grid(
        columns: 5,
        stroke: 1pt,
        inset: .5em,
        [UINV], [PIR], [NDST], [ON], [SN],
      ),
      name: "upid",
    )

    content((rel: (0, .4em), to: "upid.north-west"), anchor: "south-west", [UPID
      (receiver thread)])

    content(
      (rel: (-.5, -2), to: "upid.south-west"),
      anchor: "north-west",
      grid(
        columns: (5em, auto, 5em),
        stroke: 1pt,
        inset: .5em,
        align: center,
        math.dots, [1], math.dots,
      ),
      name: "pir",
    )

    content(
      (rel: (0, -.4em), to: "pir.south-west"),
      anchor: "north-west",
      [PUIR],
    )

    line(
      (rel: (.3em, 0), to: "senduipi.east"),
      (rel: (-.3em, -.98), to: "uitt.north-west"),
      mark: (end: ">"),
    )

    bezier(
      (rel: (1.1, -.65), to: "uitt.north-west"),
      (rel: (-.3em, -.33), to: "upid.north-west"),
      (rel: (1.1, -.2), to: "uitt.north-west"),
      (rel: (-.3em - 1cm, -.33), to: "upid.north-west"),
      mark: (end: ">"),
    )

    line(
      (rel: (1.3, 0), to: "upid.south-west"),
      "pir.north-west",
    )

    line(
      (rel: (2.22, 0), to: "upid.south-west"),
      "pir.north-east",
    )

    bezier(
      (rel: (0, -1), to: "uitt.north-east"),
      (rel: (0, .3em), to: "pir.north"),
      (rel: (.6, -1), to: "uitt.north-east"),
      (rel: (0, .3em + 1cm), to: "pir.north"),
      mark: (end: ">"),
    )
  }),
  caption: [UINTR structures],
) <uintr-structures>

A receiver thread has a _User Posted-Interrupt Descriptor_ (UPID) registered
with the CPU that is managed by the OS. It contains the following fields:

- The _User-Interrupt Notification Vector_ (UINV) is the interrupt vector that
  will be sent to the receiving CPU.
- The _Posted-Interrupt Requests_ (PIR) contains one bit for each user-interrupt
  vector. If a bit is set, then there is a user-interrupt request for the
  corresponding vector.
- The _Notification Destination_ (NDST) identifies the CPU to send the interrupt
  to.
- The _Outstanding Notification_ (ON) bit is set when there is one or more
  outstanding notification in the PIR.
- The _Suppress Notification_ (SN) bit, when set, asks senders not to send
  UINTRs.

A sender thread has a _User-Interrupt Target Table_ (UITT) registered with the
CPU and managed by the OS. The `senduipi` instruction takes an index into this
table. UITT entries are called UITTEs. Each UITTE contains a pointer to the
receiver thread's UPID as well as a user-interrupt vector.

#paragraph-heading[Current Uses.] Opportunities of UINTR are being studied by
the research community@kone25@guo25. The xUI paper@aydogmus25 deconstructs
Intel's microarchitectural implementation of UINTRs and proposes four new
enhancements to UINTRs, each evaluated by implementing them in the gem5
simulator@gem5.

// Also, one of my classmates is trying to make use of UINTR to implement low-latency communication between virtual machines.

// #inline-note[Add Pegasus@peng25, LibPreemptible@li24, Cross-Core Interrupt Detection: Exploiting User and Virtualized IPIs@rauscher24.]

// COMMENT-RENAUD: Pas sûr de voir pourquoi tu parles du stage de Gaetan. Si tu veux vraiment en parler, il faut te positioner explicitement pour que le lecteur comprenne le lien (au minimum en disant que ces travaux sont orthogonaux).

= Motivation

Existing mechanisms to delegate page-fault handling to user space are costly in
latency. To understand these costs, let's follow the journey of a page fault,
specifically a _minor_ page fault. A minor page fault is a page fault that is
resolved without having to wait for secondary storage (i.e., typically, to read
a page from the disk): minor page faults are the simplest and fastest to
resolve.

// COMMENT-RENAUD: Pas sûr que userfaultfd ait été assez introduit pour comprendre ce qui suit.

== The Journey of a Page Fault to User Space

#paragraph-heading[For `userfaultfd` in FD Notification Mode.] Our a
`userfaultfd` thread continuously reads from the `userfaultfd` file descriptor.
When a page fault occurs, the read returns and the `userfaultfd` thread does a
`UFFDIO_ZEROPAGE` operation to resolve the page fault with the zero page and
wake up the faulting thread, as represented in @userfaultfd-fd-mode-flow.

#figure(
  execution(
    (
      [Faulting Thread],
      user-stmt[Faulting Instruction],
      internal-stmt[CPU Exception],
      kernel-stmt[Save State],
      kernel-stmt[Read-Lock VMA],
      kernel-stmt[Walk Page Table],
      kernel-stmt[Wake Up `userfaultfd`~Thread],
      (set-event: "userfaultfd-readable"),
      (wait-for-event: "userfaultfd-handled"),
      kernel-stmt[Return from `handle_mm_fault`],
      (set-event: "treat-fault-end"),
      kernel-stmt[Read-Lock VMA],
      kernel-stmt[Walk Page Table],
      kernel-stmt[Return from `handle_mm_fault`],
      kernel-stmt[Prepare for Exit],
      kernel-stmt[Restore State],
      kernel-stmt[`IRET`],
    ),
    (
      [`userfaultfd` Thread],
      (wait-for-event: "userfaultfd-readable"),
      kernel-stmt[Return from `read`],
      (set-event: "treat-fault-start"),
      user-stmt[`SYSCALL` (`ioctl`)],
      kernel-stmt[Save State],
      kernel-stmt[Read-Lock VMA],
      kernel-stmt[Walk Page table],
      kernel-stmt[Set PTE],
      kernel-stmt[Notify App Thread],
      (set-event: "userfaultfd-handled"),
    ),
    braces: (
      (
        "treat-fault-start",
        "treat-fault-end",
        box(width: 7em)[Treat Fault and Return from `handle_mm_fault`],
      ),
    ),
  ),
  caption: [Flow of `userfaultfd`-based page fault handling with the FD
    notification mode.],
) <userfaultfd-fd-mode-flow>

The kernel read-locks the VMA and walks the page table three times. In between
the notification and the `UFFDIO_ZEROPAGE` operation, the VMA that contains the
faulting address could have been modified from another thread so the kernel
verifies that the VMA is still associated with the `userfaultfd` context, and
walks the page table a second time.

#paragraph-heading[For `userfaultfd` in Signal Notification Mode.] ExtMem
configures `userfaultfd` in signal notification mode.

#figure(
  execution((
    [Faulting Thread],
    user-stmt[Faulting Instruction],
    internal-stmt[CPU Exception],
    kernel-stmt[Save State],
    kernel-stmt[Read-Lock VMA],
    kernel-stmt[Walk Page Table],
    kernel-stmt[Return from `handle_mm_fault`],
    kernel-stmt[Prepare for Exit],
    kernel-stmt[Restore State],
    kernel-stmt[`IRET`],
    user-stmt[`SYSCALL` (`ioctl`)],
    kernel-stmt[Save State],
    kernel-stmt[Read-Lock VMA],
    kernel-stmt[Walk Page table],
    kernel-stmt[Set PTE],
    kernel-stmt[Prepare for Exit],
    kernel-stmt[Restore State],
    kernel-stmt[`SYSRET`],
    user-stmt[`SYSCALL` (`rt_sigreturn`)],
    kernel-stmt[Save State],
    kernel-stmt[Configure Registers for Signal Return],
    kernel-stmt[Prepare for Exit],
    kernel-stmt[Restore State],
    kernel-stmt[`SYSRET`],
  )),
  caption: [Flow of `userfaultfd`-based page fault handling with the signal
    notification mode.],
) <userfaultfd-signal-mode-flow>

=== For USM

#figure(
  execution(
    (
      [Faulting Thread],
      user-stmt[Faulting Instruction],
      internal-stmt[CPU Exception],
      kernel-stmt[Save State],
      kernel-stmt[Read-Lock VMA],
      kernel-stmt[Walk Page Table],
      kernel-stmt[RPC to Resolve Fault],
      (set-event: "rpc-start"),
      (wait-for-event: "rpc-end"),
      kernel-stmt[Return from `handle_mm_fault`],
      kernel-stmt[Prepare for Exit],
      kernel-stmt[Restore State],
      kernel-stmt[`IRET`],
    ),
    (
      [USM Thread],
      (wait-for-event: "rpc-start"),
      user-stmt[Allocate Physical~Page],
      user-stmt[Return from RPC],
      (set-event: "rpc-end"),
    ),
    braces: (
      (
        "rpc-start",
        "rpc-end",
        box(width: 6em)[Wait for RPC to Return],
      ),
    ),
  ),
  caption: [Flow of USM-based page fault handling.],
)

== Problem Assessment

We validate our intuition with benchmarks. Since Linux lazily-allocates memory,
one way to trigger a minor page fault is by allocating a page and read from or
write to it, as can be seen in @microbenchmark-excerpt.

#figure(caption: [Triggering a minor page fault.])[
  ```c
  void *page = mmap(NULL, page_size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  /* Read the first byte of the page. */
  *(volatile char *)page;
  ```
] <microbenchmark-excerpt>

#figure(
  table(
    columns: 3,
    table.header([], [], [USM]),
    table.cell(rowspan: 11, align: horizon, rotate(-90deg, reflow: true)[
      TSC cycles
    ]),
    [CPU Exception], $575$,
    [Save State], $369$,
    [Read-Lock VMA], $341$,
    [Walk Page Table], $195$,
    [RPC to Resolve Fault], $568$,
    [Wait for RPC to Return], $985$,
    [Return from `handle_mm_fault`], $386$,
    [Prepare for Exit], $156$,
    [Restore State], $132$,
    [`IRET`], $333$,
    [Total], $4184$,
  ),
  caption: [Breakdown of the minor page fault flow for USM.],
)

COMMENT-RENAUD: Cette partie semble incomplète/inachevée et il manque notamment
une discussion/analyse concernant les résultats de la Table 1.


= UserFault (UF) Design

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

Following an existing trend of kernel bypass---the idea of having applications
directly communicate with the hardware to improve performance---we propose a
hardware modification to let applications directly handle page faults without
going through the kernel first, as can be seen in @flow-of-privilege.

#let page-fault-privilege-flow-diagram(bypass-kernel) = cetz.canvas({
  import cetz.draw: *

  content(
    (),
    box(stroke: 1pt, inset: .5em)[CPU],
    name: "cpu",
  )

  content(
    (rel: (2, 0), to: "cpu.east"),
    anchor: "west",
    [Handler],
    name: "handler",
  )

  line(
    (rel: (0, 1), to: ("cpu.east", 50%, "handler.west")),
    (rel: (0, -1), to: ("cpu.east", 50%, "handler.west")),
    name: "border",
  )

  content((rel: (-.5em, 0), to: "border.start"), anchor: "north-east", [K])

  content((rel: (.5em, 0), to: "border.start"), anchor: "north-west", [U])

  if bypass-kernel {
    line(
      "cpu",
      (rel: (-.5em, 0), to: "handler.west"),
      mark: (end: ">"),
      name: "path",
    )
  } else {
    line(
      "cpu",
      (rel: (-1em, 0), to: "border.mid"),
      mark: (end: ">"),
      name: "path",
    )

    line(
      (rel: (-.5em, 0), to: "border.mid"),
      (rel: (-.5em, 0), to: "handler.west"),
      mark: (end: ">"),
      name: "path",
    )
  }

  content(
    (rel: (.5em, -.5em), to: "cpu.east"),
    anchor: "north-west",
    `#PF`,
  )
})

#subpar.grid(
  columns: 2,
  column-gutter: 1.5cm,
  figure(page-fault-privilege-flow-diagram(false), caption: [Without UF]),
  figure(page-fault-privilege-flow-diagram(true), caption: [With UF]),

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

== Preliminary Prototype

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

The RDTSC instruction is not serializing: the CPU is free to reorder previous
and subsequent instructions to make full use of its instruction pipeline. We use
_fences_ to prevent instruction reordering around the RDTSC instruction as
suggested by the manual.

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
  pl $0xb141a52a, %ebx
  ```
]
