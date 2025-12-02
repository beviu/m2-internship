#import "@preview/touying:0.6.0": *
#import themes.simple: *
#import "@preview/bytefield:0.0.7": *
#import "@preview/cetz:0.3.2"

#set document(description: [First presentation of what I plan to work on during
  my internship to the KrakOS team.])

#show: simple-theme.with(aspect-ratio: "16-9", primary: purple, config-info(
  title: [Hardware assisted user-space memory management (uMM)],
  author: [Greg Depoire-\-Ferrer],
  date: datetime(
    year: 2025,
    month: 2,
    day: 27,
    hour: 14,
    minute: 0,
    second: 0,
  ),
  institution: [KrakOS],
))

#title-slide[
  = Hardware assisted user-space memory management (uMM)

  #v(2em)

  #grid(
    columns: (auto, auto),
    column-gutter: 2em,
    [
      *Student:* \
      Greg Depoire-\-Ferrer \
      #link("mailto:greg.depoire--ferrer@ens-lyon.fr")
    ],
    [
      *Supervised by:* \
      Alain Tchana \
      Renaud Lachaize
    ],
  )

  #v(1em)

  #let logo = rect(fill: black, image("krakos.png"), inset: .1em)

  #grid(
    columns: (auto, auto),
    column-gutter: 2em,
    [February 27, 2025], logo,
  )

  #speaker-note[
    I'm going to present what I plan to work on during my internship.
  ]
]

= Background

== User-space memory management

// Touying has a bug
// (https://forum.typst.app/t/how-do-i-add-bibliography-to-a-touying-presentation/643/7) which makes
// this code snippet create a blank slide if it's put in the preamble, so put it later.
#show: magic.bibliography-as-footnote.with(bibliography(
  "bibliography.yaml",
  title: none,
))

A need for more *flexibility* in memory management.

#speaker-note[
  There is no *one size fits all* policy for memory management. There is recent
  work to move a part of memory management to user space so that it can be
  modified more easily.

  `userfaultfd` is a Linux functionality to let user space process page faults.
]

- ExtMem [ATC '24]
- USM (PhD Assane Fall, KrakOS)
- `userfaultfd`-based systems

== A tendency of user-space delegation

- ghOSt [SOSP '21] (CPU)
- uFS [SOSP '21] (FS)
- Bento [FAST '21] (FS)
- Concord [SOSP '23] (Network)
- Demikernel [SOSP '21] (Network)
- Snap [SOSP '19] (Network)
- etc.

#speaker-note[
  Delegating services to user-space is not a new idea. It is commonly done for
  devices and IO like networking devices, disks and filesystems. It has also
  been done for scheduling with ghOSt.

  The goal is to improve application performance by using policies specifically
  tuned for the application. The overhead introduced by delegating the service
  to user-space therefore has to be minimal.
]

== Current limitations

#grid(
  columns: (8cm, 1fr),
  column-gutter: .4em,
  [
    === Implementations

    - `SIGSEGV` handling
    - `userfaultfd`@userfaultfd-doc
    - ExtMem [ATC '24]
    - USM (PhD Assane~Fall, KrakOS)

    #speaker-note[
      Back to the case of memory management, the current implementations suffer
      from a limitation.
    ]

    #pause
  ],
  [
    === Hardware limitations

    Unlike networking and disk IO, *kernel bypass* is not possible #math.arrow.r
    new hardware features needed.

    #speaker-note[
      Kernel bypass is a technique used to make user-space delegation usable.
      Without it, the service needs to ask the kernel before doing anything
      which introduces too much overhead.
    ]

    #align(center, cetz.canvas({
      import cetz.draw: *

      let part(body) = box(height: 1.5em, inset: (x: .5em), align(
        horizon,
        body,
      ))

      content((), frame: "rect", part[uMM], name: "umm")
      content(
        (rel: (-1, 0), to: "umm.west"),
        frame: "rect",
        part[Kernel],
        name: "kernel",
        anchor: "east",
      )
      content(
        (rel: (-1, 0), to: "kernel.west"),
        frame: "rect",
        part[CPU],
        name: "cpu",
        anchor: "east",
      )

      content(
        (rel: (0, -1), to: "umm.south"),
        frame: "rect",
        part[Bento],
        name: "bento",
        anchor: "north",
      )
      content(
        (rel: (0, -1), to: "cpu.south"),
        frame: "rect",
        part[IO],
        name: "io",
        anchor: "north",
      )

      line(
        (
          rel: (0, .5),
          to: ("umm.north-west", .5, "kernel.north-east"),
        ),
        (
          horizontal: (),
          vertical: (rel: (0, -.5), to: "bento.south-west"),
        ),
        stroke: (dash: "dashed", paint: gray),
      )

      line(
        (
          rel: (0, .5),
          to: ("cpu.north-east", .5, "kernel.north-west"),
        ),
        (
          horizontal: (),
          vertical: (rel: (0, -.5), to: "io.south-east"),
        ),
        stroke: (dash: "dashed", paint: gray),
      )

      line("umm", "kernel", mark: (symbol: ">"))
      line("kernel", "cpu", mark: (symbol: ">"))
      bezier(
        "umm.south",
        "cpu.south",
        (rel: (0, -1.5), to: ("umm.south", 50%, "cpu.south")),
        stroke: red + 2pt,
        mark: (symbol: ">"),
      )

      line("bento", "io", mark: (symbol: ">"))
    }))
  ],
)

= Plan

== Plan of the internship

+ *Motivation*: evaluate the cost of existing approaches and approximate the
  cost of the proposed approaches using a model.
+ *Implementation* and *evaluation*.

= Memory management costs

#speaker-note[
  The rest of the slides focus on the cost of page fault handling, which can be
  significant cost in memory-intensive applications and is what we want to
  improve.
]

== Minor page fault

#align(horizon)[
  ```c
  volatile char *page = mmap(
    NULL, 4096, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  ```

  #v(1em)

  #stack(
    text(.6em, weight: "bold", pad(bottom: .75em, [App thread])),
    ```c
    /* Page fault. */
    *page;
    ```,
  )
]

== Linux memory management

#let statement(body, color: none, bold: false) = {
  let fill = if bold {
    color.lighten(60%)
  } else {
    color.lighten(75%).desaturate(25%)
  }

  box(fill: fill, inset: (x: .4em, y: .5em), radius: .5em, text(
    color.darken(50%),
    body,
  ))
}

#let kernel-space-statement = statement.with(color: red)
#let user-space-statement = statement.with(color: blue)
#let internal-statement = statement.with(color: green)

#let statement-sequence(..statements) = stack(spacing: .25em, ..statements)

#let execution(..threads) = {
  let headers = threads
    .pos()
    .map(thread => pad(
      text(weight: "bold", thread.name),
      bottom: .5em,
    ))

  let thread-statements = threads.pos().map(thread => thread.statements)
  let statements = thread-statements
    .at(0)
    .zip(..thread-statements.slice(1), exact: true)
    .flatten()

  text(.4em, grid(
    columns: threads.pos().map(_thread => auto),
    column-gutter: 1em,
    row-gutter: .25em,
    ..headers,
    ..statements,
  ))
}

#let linux-page-fault-handling(
  find-physical-page-bold: false,
  references: true,
) = execution((
  name: [App thread],
  statements: (
    user-space-statement[Faulting instruction],
    internal-statement[Exception],
    kernel-space-statement[Save state],
    kernel-space-statement[Search for VMA],
    kernel-space-statement(bold: find-physical-page-bold)[Find physical page],
    kernel-space-statement[Update PTE],
    kernel-space-statement[Restore state],
    kernel-space-statement[`iret`#if references {
        ref(<torvalds-page-fault-cost>)
      }],
  ),
))

#columns(2, [
  #alternatives(
    linux-page-fault-handling(),
    linux-page-fault-handling(
      find-physical-page-bold: true,
    ),
  )

  ==== Legend

  #kernel-space-statement[Kernel space]
  #user-space-statement[User space]
  #internal-statement[CPU internals]

  #only(2)[Policy is hard to change.]
])

#speaker-note[
  Present the legend. List the parts, one by one.
]

== `userfaultfd` memory management

#let userfaultfd-page-fault-handling = execution(
  (
    name: [App thread],
    statements: (
      statement-sequence(
        user-space-statement[Faulting instruction],
        internal-statement[Exception],
        kernel-space-statement[Save state],
        kernel-space-statement[Search for VMA],
        kernel-space-statement[Notify memory thread],
      ),
      kernel-space-statement[Wait],
      statement-sequence(
        kernel-space-statement[Restore state],
        kernel-space-statement[`iret`],
      ),
    ),
  ),
  (
    name: [Memory thread],
    statements: (
      [],
      statement-sequence(
        kernel-space-statement[Complete `read`],
        kernel-space-statement[Restore state],
        kernel-space-statement[`sysret`],
        user-space-statement[Find physical page],
        user-space-statement[`syscall`],
        kernel-space-statement[Save state],
        kernel-space-statement[Update PTE],
        kernel-space-statement[Notify app thread],
      ),
      [],
    ),
  ),
)

#columns(2, [
  #userfaultfd-page-fault-handling

  #let direct-latency-costs = [
    === Latency costs

    - 2 context switches
    - 1 transition from kernel space to user space
    - 1 system call
  ]

  #alternatives(
    [
      === Linux memory management

      #linux-page-fault-handling(references: false)
    ],
    direct-latency-costs,
    [
      #direct-latency-costs
      - #link(<indirect-costs>)[*Indirect costs*]
    ],
  )
])

== Indirect costs <indirect-costs>

+ CPU cache pollution@flexsc
+ Speculative execution barrier

  #text(gray, .75em, quote(attribution: <intel-manual>)[
    *Instruction ordering.* Instructions following a SYSCALL may be fetched from
    memory before earlier instructions complete execution, but they will not
    execute (even speculatively) until all instructions prior to the SYSCALL
    have completed execution (the later instructions may execute before data
    stored by the earlier instructions have become globally visible).
  ])

Harder to measure.

== Workflow of the solution

- Modify the hardware to make *kernel bypass* possible.

#pause

=== Hardware modifications

- By default, page faults continue to be handled by the kernel. #pause
- VMAs for which page faults must be handled in user space are tagged.

== First version of user-interrupt memory management

#let user-interrupt-synchronous-page-fault-handling = execution((
  name: [App thread],
  statements: (
    user-space-statement[Faulting instruction],
    internal-statement[User page fault check],
    internal-statement[Post & deliver user interrupt],
    user-space-statement[Save state],
    user-space-statement[Find physical page],
    user-space-statement[`syscall`],
    kernel-space-statement[Save state],
    kernel-space-statement[Update PTE],
    kernel-space-statement[Restore state],
    kernel-space-statement[`sysret`],
    user-space-statement[Restore state],
  ),
))

#columns(2, [
  #user-interrupt-synchronous-page-fault-handling

  #colbreak()

  #alternatives(
    [
      === `userfaultfd` PF handling

      #text(.9em, userfaultfd-page-fault-handling)
    ],
    [
      === Latency costs

      - #strike[2 context switches]
      - #strike[1 transition from kernel space to user space]
      - 1 system call
      - Unknown hardware costs
    ],
  )
])

== `io_uring`-based user-interrupt memory management

#slide(repeat: 4, columns(2, [
  #execution(
    (
      name: [App thread],
      statements: (
        statement-sequence(
          user-space-statement[Faulting instruction],
          {
            only((until: 3), internal-statement[User page fault check])
            only(4, internal-statement(bold: true)[User page fault check])
          },
          internal-statement[Post & deliver user interrupt],
          user-space-statement[Save state],
          user-space-statement[Find physical page],
          alternatives(
            user-space-statement(bold: true)[Start async. PTE update],
            user-space-statement[Start async. PTE update],
            repeat-last: true,
          ),
        ),
        alternatives(
          user-space-statement(
            bold: true,
          )[Do work on a different coroutine],
          user-space-statement[Do work on a different coroutine],
          repeat-last: true,
        ),
        user-space-statement[Restore state],
      ),
    ),
    (
      name: [`io_uring` kthread],
      statements: (
        [],
        kernel-space-statement[Update PTE],
        [],
      ),
    ),
  )

  #colbreak()

  #let latency-costs = [
    === Latency costs

    - #text(gray, strike[2 context switches])
    - #text(gray, strike[1 transition from kernel space to user space])
    - #strike[1 system call]
    - Unknown hardware costs
  ]

  #alternatives(
    [
      === Without `io_uring`

      #user-interrupt-synchronous-page-fault-handling
    ],
    latency-costs,
    [
      #latency-costs

      === Costs not on the critical path

      - New `io_uring` kthread.
    ],
  )
]))

= User page fault check

== User-interrupt PTEs

#text(12pt, bytefield(
  msb: left,
  bitheader("bounds"),
  flag("XD"),
  bits(4, "PK"),
  bits(7, "free", fill: yellow.lighten(50%)),
  bits(40, "physical address"),
  bits(3, "free", fill: yellow.lighten(50%)),
  flag("G"),
  flag("PAT"),
  flag("D"),
  flag("A"),
  flag("PCD"),
  flag("PWT"),
  flag("U/S"),
  flag("R/W"),
  flag("1"),
))

#text(12pt, bytefield(
  msb: left,
  bitheader("offsets"),
  bits(63, "free", fill: yellow.lighten(50%)),
  flag("0"),
))

Modify PTE format to store whether CPU must send a user interrupt whenever an
access is made or not.

== User interrupt fault range

#grid(
  columns: (1fr, auto),
  column-gutter: 1em,
  [
    - New MSRs that define a range of memory for which every page fault results
      in a user interrupt.
    - Handler won't know if the faulting address is inside a valid VMA (it might
      or might not keep a record of the valid VMAs).
  ],
  grid(
    inset: .5em,
    stroke: black,
    align: center,
    v(.25cm),
    grid.cell(fill: red.lighten(50%), [Code]),
    grid.cell(fill: orange.lighten(50%), [Data]),
    grid.cell(fill: purple.lighten(50%), [Heap]),
    v(.5cm),
    grid.cell(fill: black, v(2cm)),
    v(.25cm),
  ),
)

#focus-slide[
  Thank you. \
  Any questions?

  #speaker-note[
    eBPF is not flexible enough: for example, you cannot implement complex data
    structures with eBPF.
  ]
]

#appendix[
  == FRED

  #quote(attribution: <fred-spec>)[
    The FRED architecture was designed with the following goals:
    - Improve overall performance and response time by *replacing event
      delivery* through the interrupt descriptor table (IDT event delivery) and
      *event return* by the IRET instruction with lower latency transitions.
  ]

  But no supporting hardware yet!

  == Some measurements

  #table(
    columns: (auto, auto),
    table.header([], [Minimum (cycles)]),
    [Page fault #math.arrow.r resumption], [1271],
    [Page fault #math.arrow.r `SIGSEGV` handler], [2254],
    [`mprotect`], [2929],
    [Return from `SIGSEGV` handler], [902],
  )

  On Intel Core i5-1335U.
]
