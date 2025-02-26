#import "@preview/touying:0.6.0": *
#import themes.simple: *
#import "@preview/bytefield:0.0.7": *
#import "@preview/cetz:0.3.2"

#set document(
  description: [First presentation of what I plan to work on during my internship to the KrakOS team.],
)

#show: simple-theme.with(
  aspect-ratio: "16-9",
  config-info(
    title: [Hardware assisted user-space page fault handling],
    author: [Greg Depoire-\-Ferrer],
    date: datetime(
      year: 2025,
      month: 2,
      day: 27,
      hour: 14,
      minute: 0,
      second: 0,
    ),
  ),
)

#title-slide[
  = Hardware-assisted user-space page fault handling

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
      #link("mailto:alain.tchana@grenoble-inp.fr")
    ],
  )

  #v(2em)

  February 27, 2025
]

= Background

== Minor page fault

// Touying has a bug
// (https://forum.typst.app/t/how-do-i-add-bibliography-to-a-touying-presentation/643/7) which makes
// this code snippet create a blank slide if it's put in the preamble, so put it later.
#show: magic.bibliography-as-footnote.with(
  bibliography("bibliography.yaml", title: none),
)

#align(horizon)[
  ```c
  page = mmap(
    NULL, 4096, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  ```

  #v(1em)

  #stack(
    text(.6em, weight: "bold", pad(bottom: .75em, [App thread])),
    ```c
    /* Page fault. */
    *(volatile char *)page;
    ```,
  )
]

== Linux page fault handling

#let statement(body, color: none, bold: false) = {
  let fill = if bold {
    color.lighten(60%)
  } else {
    color.lighten(75%).desaturate(25%)
  }

  box(
    fill: fill,
    inset: (x: .4em, y: .5em),
    radius: .5em,
    text(color.darken(50%), body),
  )
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

  text(
    .4em,
    grid(
      columns: threads.pos().map(_thread => auto),
      column-gutter: 1em,
      row-gutter: .25em,
      ..headers,
      ..statements,
    ),
  )
}

#columns(
  2,
  [
    #execution((
      name: [App thread],
      statements: (
        user-space-statement[Faulting instruction],
        internal-statement[Exception],
        kernel-space-statement[Save state],
        kernel-space-statement[Search for VMA],
        alternatives(
          kernel-space-statement[Find physical page],
          kernel-space-statement(bold: true)[Find physical page],
        ),
        kernel-space-statement[Update PTE],
        kernel-space-statement[Restore state],
        kernel-space-statement[`iret`@torvalds-page-fault-cost],
      ),
    ))

    ==== Legend

    #kernel-space-statement[Kernel space]
    #user-space-statement[User space]
    #internal-statement[CPU internals]

    #only(2)[Policy is hard to change.]
  ],
)

== User space page fault handling

#columns(
  2,
  [
    #execution(
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

    #pause

    === Latency costs

    - 2 context switches
    - 1 transition from kernel space to user space
    - 1 system call
  ],
)

== User interrupt page fault handling

#columns(
  2,
  [
    #execution((
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

    #colbreak()

    #pause

    === Latency costs

    - 1 system call
    - Unknown hardware costs
  ],
)

== User interrupt page fault handling (asynchronous)

#columns(
  2,
  [#execution(
      (
        name: [App thread],
        statements: (
          statement-sequence(
            user-space-statement[Faulting instruction],
            alternatives(
              internal-statement[User page fault check],
              internal-statement(bold: true)[User page fault check],
            ),
            internal-statement[Post & deliver user interrupt],
            user-space-statement[Save state],
            user-space-statement[Find physical page],
            alternatives(
              user-space-statement(bold: true)[Start async. PTE update],
              user-space-statement[Start async. PTE update],
            ),
          ),
          alternatives(
            user-space-statement(bold: true)[Do work on a different coroutine],
            user-space-statement[Do work on a different coroutine],
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

    === Latency costs

    - Unknown hardware costs
  ],
)

= User page fault check

== User-interrupt PTEs

#text(
  12pt,
  bytefield(
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
  ),
)

#text(
  12pt,
  bytefield(
    msb: left,
    bitheader("offsets"),
    bits(63, "free", fill: yellow.lighten(50%)),
    flag("0"),
  ),
)

Modify PTE format to store whether CPU must send a user interrupt whenever an access is made or not.

== User interrupt fault range

#grid(
  columns: (1fr, auto),
  column-gutter: 1em,
  [
    - New MSRs that define a range of memory for which every page fault results in a user interrupt.
    - Handler won't know if the faulting address is inside a valid VMA (it might or might not keep a
      record of the valid VMAs).
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

= Evaluating latency costs

== Direct costs

#table(
  columns: (auto, auto),
  table.header([], [Minimum (cycles)]),
  [Page fault #math.arrow.r resumption], [1271],
  [Page fault #math.arrow.r `SIGSEGV` handler], [2254],
  [`mprotect`], [2929],
  [Return from `SIGSEGV` handler], [902],
)

#pause

== Indirect costs

+ CPU cache pollution
+ Speculative execution barrier

  #text(
    gray,
    .75em,
    quote(attribution: <intel-manual>)[
      *Instruction ordering.* Instructions following a SYSCALL may be fetched from memory before earlier instructions
      complete execution, but they will not execute (even speculatively) until all instructions prior to the SYSCALL have
      completed execution (the later instructions may execute before data stored by the earlier instructions have
      become globally visible).
    ],
  )

== FRED

#quote(attribution: <fred-spec>)[
  The FRED architecture was designed with the following goals:
  - Improve overall performance and response time by *replacing event delivery*
    through the interrupt descriptor table (IDT event delivery) and *event return* by
    the IRET instruction with lower latency transitions.
]

But no supporting hardware yet!
