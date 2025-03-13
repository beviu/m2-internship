#import "@preview/cetz:0.3.3"
#import "@preview/drafting:0.2.2": *
#import "simple-arrow.typ": simple-arrow

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

+ *Exception*: the processor detects the page fault and jumps to the operating system exception handler.
+ *Save state*: the operating system saves the registers of the program that generated the page fault.
+ *Search for VMA*: the operating system searches for the VMA that contains the faulting virtual address.
+ *Handle fault*: the operating system fills in the physical page number in the PTE.
+ *Restore state*: the operating system restores the register to the ones that were saved before.
+ *IRET*: the operating system jumps back to the code that was executing before the page fault with the IRET (Interrupt RETurn) instruction.

The measurement methodology is described in @execution-time-measurement-method.
The results are shown in @linux-page-fault-breakdown.

#let array-differences(array) = ((0,) + array).zip(array).map(((a, b)) => b - a)

#let timings-csv(source) = {
  let rows = csv(source)
  let keys = rows.at(0).map(key => key.trim())
  let minimums = (:)
  for row in rows.slice(1) {
    let durations = array-differences(row.map(value => int(value.trim())))
    for (key, duration) in keys.zip(durations) {
      if minimums.keys().contains(key) {
        minimums.at(key) = calc.min(minimums.at(key), duration)
      } else {
        minimums.insert(key, duration)
      }
    }
  }
  minimums
}

#let linux-page-fault-timings = timings-csv("linux-page-fault-timings/results.txt")

#let linux-page-fault-breakdown-table(timings) = {
  let total = (
    timings.save-state-start
      + timings.save-state-end
      + timings.search-for-vma-end
      + timings.handle-fault-end
      + timings.iret
  )
  let m(number) = math.equation(str(number))
  table(
    columns: (auto, auto),
    table.header([Operation], [Minimum (cycles)]),
    [Exception], m(timings.save-state-start),
    [Save state], m(timings.save-state-end),
    [Search for VMA], m(timings.search-for-vma-end),
    [Handle fault], m(timings.handle-fault-end),
    [Restore state], m(timings.iret),
    [IRET], m(timings.end),
    [Total], m(total),
  )
}

#figure(
  caption: [Linux page fault execution time breakdown],
  linux-page-fault-breakdown-table(linux-page-fault-timings),
) <linux-page-fault-breakdown>

#inline-note[
  Rerun the experiment on the test machine and update the environment in the
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

#let stmt(body, base-color: none, bold: false) = {
  let fill = if bold {
    base-color.lighten(60%)
  } else {
    base-color.lighten(75%).desaturate(25%)
  }

  rect(
    width: 10em,
    fill: fill,
    stroke: black,
    inset: (x: .4em, y: .5em),
    align(center, body),
  )
}

#let kernel-stmt = stmt.with(base-color: red)
#let user-stmt = stmt.with(base-color: blue)
#let internal-stmt = stmt.with(base-color: green)

#let get-location(callback) = {
  let label = label("loc-tracker")

  [
    #metadata(none) // A label must be attached to some content.
    #label
  ]

  context {
    let location = query(selector(label).before(here())).last().location()
    callback(location)
  }
}

#let execution(..args) = context {
  let threads = args.pos()

  let headers = threads.map(thread => {
    let thread-name = thread.at(0)
    strong(thread-name)
  })

  let columns = threads.map(thread => {
    let thread-statements = thread.slice(1)
    let elements = thread-statements.map(statement => {
      if type(statement) == content {
        statement
      } else if (
        type(statement) == dictionary
          and statement.keys().contains("wait-for-event")
      ) {
        let event-name = statement.at("wait-for-event")
        let event-label = label("execution-" + event-name)
        get-location(location => {
          let current-position = location.position()
          let event-set-position = query(selector(event-label))
            .last()
            .location()
            .position()
          let y = current-position.y
          if y < event-set-position.y {
            block(height: event-set-position.y - y)
            y = event-set-position.y
          }
          place(
            simple-arrow(
              start: (
                event-set-position.x - current-position.x,
                event-set-position.y - y,
              ),
              end: (0pt, 0pt),
              thickness: .1em,
            ),
          )
        })
      } else if (
        type(statement) == dictionary and statement.keys().contains("set-event")
      ) {
        let event-name = statement.at("set-event")
        let event-label = label("execution-" + event-name)
        [
          #metadata(none) // A label must be attached to some content.
          #event-label
        ]
      } else {
        panic(
          "statement is neither content, nor a dictionary with a \"set-event\" or \"wait-for-event\" key:",
          statement,
        )
      }
    })
    stack(
      dir: ttb,
      spacing: .4em,
      ..elements,
    )
  })

  table(
    columns: threads.len(),
    stroke: (x, y) => if threads.len() > 0 and x < threads.len() - 1 {
      (right: (dash: "dotted"))
    },
    table.header(..headers),
    ..columns,
  )
}

#figure(
  execution(
    (
      [Faulting thread],
      user-stmt[Faulting instruction],
      internal-stmt[Exception],
      kernel-stmt[Save state],
      kernel-stmt[Search for VMA],
      kernel-stmt[Notify memory thread],
      (set-event: "userfaultfd-readable"),
      (wait-for-event: "userfaultfd-handled"),
      kernel-stmt[Restore state],
      kernel-stmt[IRET],
    ),
    (
      [`userfaultfd` thread],
      (wait-for-event: "userfaultfd-readable"),
      kernel-stmt[Return from `read`],
      user-stmt[`syscall`],
      kernel-stmt[Save state],
      kernel-stmt[Update PTE],
      kernel-stmt[Notify app thread],
      (set-event: "userfaultfd-handled"),
    ),
  ),
  caption: [Flow of `userfaultfd`-based page fault handling],
) <userfaultfd-page-fault-flow>

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
  set heading(numbering: "A.1", supplement: [Appendix])
  counter(heading).update(0)
  body
}

#show: appendix

= Measuring execution time <execution-time-measurement-method>

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
@linux-page-fault-no-fence-breakdown shows the execution time of the same
operation as measured in @linux-page-fault-breakdown, but without the fences.

#figure(
  caption: [Linux page fault execution time breakdown (without fences)],
  linux-page-fault-breakdown-table(
    timings-csv("linux-page-fault-timings/results-no-fence.txt"),
  ),
) <linux-page-fault-no-fence-breakdown>

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

#figure(
  cetz.canvas({
    import cetz.draw: *

    let width = 11
    let padding = 1

    line((0, 0), (width + 2 * padding, 0), mark: (end: ">"))

    let absolute-x(relative) = padding + relative * width

    let cumulative-timings = cumulative-sum(linux-page-fault-timings.values())
    let total = cumulative-timings.last()

    let timing-x(time) = absolute-x(time / total)

    let printk-line(start, end) = line(
      (timing-x(start), 0),
      (timing-x(end), 0),
      stroke: red + 2pt,
    )

    printk-line(cumulative-timings.at(1), cumulative-timings.at(2))
    printk-line(cumulative-timings.at(3), cumulative-timings.at(4))
    printk-line(cumulative-timings.at(5), cumulative-timings.at(6))

    let mark(time, body, color: black, bottom: false) = {
      let x = timing-x(time)

      line((x, -.2), (x, .2), stroke: color)

      let body = text(size: .75em, fill: color, body)
      if bottom {
        content((rel: (0, -.5em), to: (x, -.2)), body, anchor: "north")
      } else {
        content((rel: (0, .5em), to: (x, .2)), body, anchor: "south")
      }
    }

    mark(0, [PF], color: orange.darken(50%))
    mark(
      cumulative-timings.at(0),
      math.accent(math.text[Save], math.arrow),
      color: teal.darken(50%),
    )
    mark(
      cumulative-timings.at(1),
      math.accent(math.text[Save], math.arrow.l),
      color: teal.darken(50%),
      bottom: true,
    )
    mark(
      cumulative-timings.at(2),
      math.accent(math.text[VMA], math.arrow),
      color: fuchsia.darken(50%),
    )
    mark(
      cumulative-timings.at(3),
      math.accent(math.text[VMA], math.arrow.l),
      color: fuchsia.darken(50%),
      bottom: true,
    )
    mark(
      cumulative-timings.at(4),
      math.accent(math.text[Handle], math.arrow),
      color: navy,
    )
    mark(
      cumulative-timings.at(5),
      math.accent(math.text[Handle], math.arrow.l),
      color: navy,
      bottom: true,
    )
    mark(
      cumulative-timings.at(6),
      pad(right: .125cm, math.accent(math.text[Restore], math.arrow.r)),
      color: maroon.darken(50%),
    )
    mark(
      cumulative-timings.at(7),
      [`iret`],
      color: yellow.darken(50%),
      bottom: true,
    )
    mark(total, pad(left: .125cm)[End], color: lime.darken(50%))
  }),
  caption: [Positions of RDTSC instructions with `printk` calls in red],
)

The code for measuring the execution time of the operations that happen during
a minor page fault can be found in the `page-fault-breakdown` directory of the
repository: there is a kernel patch as well as the source code of a program
to be run on a patched kernel that produces a CSV file where each row contains
the execution time of the operations for a page fault.

== Environment

The measurements were done on machine with a _Intel Core i5-1335U_ processor and
Linux 6.14.
