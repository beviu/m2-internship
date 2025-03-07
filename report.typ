#import "@preview/cetz:0.3.3"
#import "@preview/drafting:0.2.2": *

#set heading(numbering: "1.1")

#set document(
  title: [M2 internship report],
  description: [Hardware-assisted user-space page fault handling],
  author: "Greg Depoire--Ferrer",
  date: datetime(year: 2025, month: 6, day: 1),
)

_M2 internship report_ #h(1fr) Greg #smallcaps[Depoire-\-Ferrer]

#v(1fr)

#let title(body) = align(
  center,
  block(
    width: 80%,
    align(
      center,
      [
        #line(length: 50%)
        #text(
          size: 20pt,
          weight: "bold",
          body,
        )
        #line(length: 50%)
      ],
    ),
  ),
)

#title[Hardware-assisted user-space page fault handling]

#pad(
  y: 1.5cm,
  align(
    center,
    block(
      width: 90%,
      grid(
        columns: (1fr, 1fr),
        column-gutter: 2em,
        align: horizon,
        [
          _Location_

          #v(.5em)

          #link("https://lig-krakos.imag.fr/")[KrakOS team], \
          #link("https://www.liglab.fr/")[Laboratoire d'Informatique de Grenoble~(LIG)]
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
      ),
    ),
  ),
)

#let part(body) = block(
  above: 2.5em,
  below: 1.125em,
  text(
    size: 13.2pt,
    weight: "bold",
    body,
  ),
)

#part[Abstract]

This report proposes a hardware modification to Intel CPUs to optimize the
user-space page fault handling in Linux.

#v(1fr)

#h(1fr) June 1, 2025

#pagebreak()

#{
  show outline.entry.where(level: 1): set block(above: 1.2em)
  show outline.entry.where(level: 1): set text(weight: "bold")

  show outline.entry: set outline.entry(
    fill: pad(left: .5em, right: 1em, repeat([.], gap: 0.5em)),
  )
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
as disk) is used as additional memory.

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

= Measuring the execution time of small steps

In order to evaluate the hardware modifications that we propose, we make a model
of the current page fault handling and then use it to approximate what would be
the execution time of a page fault with the hardware modifications.

To do that, we will need to measure the execution time of several small steps
during page fault handling.

We start by evaluating the cost of a minor page fault on Linux without any
modification. A _minor page fault_ occurs when the physical page is directly
ready to be mapped by the operating system, as opposed to a _major page fault_
where there is work to do before the fault can be resolved (e.g. read a physical
page from disk). Minor page faults are the fastest.

#figure(caption: [Triggering a minor page fault])[
  ```c
  void *page = mmap(NULL, page_size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  /* Read the first byte of the page. */
  *(volatile char *)page;
  ```
]

handling in the case of a minor page
fault. We are interested in the execution time in cycles of the following steps:

- Time from execution of faulting instruction to first instruction in OS
  exception handler.
- Time to save the CPU state (including registers) before the C code is called.
- Time to search for the VMA.
- Time to find a free physical page.
- Time to update the PTE.
- TIme to restore the CPU state after the C code returns.

We will instrument the kernel to measure these times.

However, we only want to instrument the code flow for a single process. To do
this, we are going to add checks that rely on the `current` pointer which is a
pointer to the task that is currently executing on the CPU.

== Results

I ran my `detailed-page-fault` benchmark inside a virtual machine on my computer
with a _Intel Core i5-1335U_ processor and Linux 6.14, and got the following
results:

#inline-note[Explain that physical page alloc is not included (zero page on read)]

#figure(
  caption: [Linux page fault execution time breakdown],
  table(
    columns: (auto, auto),
    table.header([], [Minimum (cycles)]),
    [Exception], [461],
    [Save state], [317],
    [Search for VMA], [158],
    [Handle fault], [308],
    [Restore state], [61],
    [`iret`], [255],
    [Sum], [1560],
  ),
) <linux-page-fault-breakdown>

// This is not a heading so that it does not appear in the outline.
#text(
  size: 15.4pt,
  weight: "bold",
  block(
    above: 1.29em,
    below: .54em,
    [Bibliography],
  ),
)

#bibliography("bibliography.yaml", title: none)

#let appendix(body) = {
  set heading(numbering: "A", supplement: [Appendix])
  counter(heading).update(0)
  body
}

#show: appendix

= Measuring execution time

The execution time of the operations we measure is in the order of a few
nanoseconds. We need a clock with a high frequency that is simple and fast to
read. We use the RDTSC (ReaD Time Stamp Counter) instruction. The _time stamp
counter_' is a counter driven by a clock whose frequency depends on the CPU but
is often in the order of the CPU's maximum frequency.

The RDTSC instruction is not serializing: the processor is free to reorder
previous and subsequent instructions to make full use of its instruction
pipeline. We use _fences_ to prevent instruction reordering around the RDTSC
instruction as suggested by the manual.

These fences slightly increase the execution time of the operations
that we measure but make the results easier to interpret. For example,
@linux-page-fault-no-fence-breakdown shows the execution time of the same
operation as measured in @linux-page-fault-breakdown, but without the fences.

#figure(
  caption: [Linux page fault execution time breakdown (without fences)],
  table(
    columns: (auto, auto),
    table.header([], [Minimum (cycles)]),
    [Exception], [455],
    [Save state], [319],
    [Search for VMA], [158],
    [Handle fault], [311],
    [Restore state], [61],
    [`iret`], [159],
    [Sum], [1463],
  ),
) <linux-page-fault-no-fence-breakdown>

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

#figure(
  cetz.canvas({
    import cetz.draw: *

    line((0, 0), (13, 0), mark: (end: ">"))

    line((2.4, 0), (4.8, 0), stroke: red + 2pt)
    line((5.1, 0), (7.5, 0), stroke: red + 2pt)
    line((8, 0), (10.4, 0), stroke: red + 2pt)

    let mark(x, body, color: black, bottom: false) = {
      line((x, -.2), (x, .2), stroke: color)

      let body = text(size: .75em, fill: color, body)

      if bottom {
        content((rel: (0, -.5em), to: (x, -.2)), body, anchor: "north")
      } else {
        content((rel: (0, .5em), to: (x, .2)), body, anchor: "south")
      }
    }

    mark(1, [PF], color: orange.darken(50%))
    mark(1.8, math.accent(math.text[Save], math.arrow), color: teal.darken(50%))
    mark(
      2.4,
      math.accent(math.text[Save], math.arrow.l),
      color: teal.darken(50%),
      bottom: true,
    )
    mark(
      4.8,
      math.accent(math.text[VMA], math.arrow),
      color: fuchsia.darken(50%),
    )
    mark(
      5.1,
      math.accent(math.text[VMA], math.arrow.l),
      color: fuchsia.darken(50%),
      bottom: true,
    )
    mark(
      7.5,
      math.accent(math.text[Handle], math.arrow),
      color: navy,
    )
    mark(
      8,
      math.accent(math.text[Handle], math.arrow.l),
      color: navy,
      bottom: true,
    )
    mark(
      10.4,
      pad(right: 1em, math.accent(math.text[Restore], math.arrow.r)),
      color: maroon.darken(50%),
    )
    mark(10.5, [`iret`], color: yellow.darken(50%), bottom: true)
    mark(11, pad(left: 1em)[End], color: lime.darken(50%))
  }),
  caption: [Positions of RDTSC instructions with `printk` calls in red],
)

