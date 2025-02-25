#import "@preview/touying:0.6.0": *
#import themes.simple: *
#import "@preview/bytefield:0.0.7": *

#set document(
  title: "Updates",
  author: "Greg Depoire--Ferrer",
  date: datetime(year: 2025, month: 2, day: 27),
)

#show: simple-theme.with(aspect-ratio: "16-9")

#title-slide[
  = Updates

  February 27, 2025
]

= Motivation

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
    .zip(..thread-statements.slice(1))
    .flatten()

  text(
    .6em,
    grid(
      columns: threads.pos().map(_thread => auto),
      column-gutter: 1em,
      row-gutter: .25em,
      ..headers,
      ..statements,
    ),
  )
}

#execution((
  name: [App thread],
  statements: (
    user-space-statement[Faulting instruction],
    kernel-space-statement[Save registers],
    kernel-space-statement[Search for VMA],
    alternatives(
      kernel-space-statement[Find physical page],
      kernel-space-statement(bold: true)[Find physical page],
    ),
    kernel-space-statement[Update PTE],
    kernel-space-statement[Resume],
  ),
))

#only(2)[Policy is hard to change.]

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
            kernel-space-statement[Save registers],
            kernel-space-statement[Search for VMA],
            kernel-space-statement[Notify memory thread],
          ),
          [],
          kernel-space-statement[Resume],
        ),
      ),
      (
        name: [Memory thread],
        statements: (
          [],
          statement-sequence(
            kernel-space-statement[Complete `poll`/`read`],
            user-space-statement[Find physical page],
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
  [#execution((
      name: [App thread],
      statements: (
        user-space-statement[Faulting instruction],
        internal-statement[User page fault check],
        internal-statement[Post & deliver user interrupt],
        user-space-statement[Save registers],
        user-space-statement[Find physical page],
        kernel-space-statement[Update PTE],
        user-space-statement[Resume],
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
            user-space-statement[Save registers],
            user-space-statement[Find physical page],
            user-space-statement[Start async. PTE update],
          ),
          user-space-statement[Do work on a different coroutine],
          user-space-statement[Resume],
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

== User page fault check

=== User-interrupt PTEs

#text(
  12pt,
  bytefield(
    msb: left,
    bitheader("bounds"),
    flag("XD"),
    bits(4, "PK"),
    bits(7, "free", fill: yellow),
    bits(40, "physical address"),
    bits(3, "free", fill: yellow),
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
    bits(63, "free", fill: yellow),
    flag("0"),
  ),
)

Modify PTE format to store whether CPU must send a user interrupt whenever an access is made or not.

=== User interrupt fault range

- New MSR that define a range of memory for which every page fault results in a user interrupt.
- The handler doesn't know if the address it got is associated to a valid VMA (it might or might not
  keep a record of the valid VMAs).

== Microbenchmarks

=== Direct overhead

#table(
  columns: (auto, auto),
  table.header([], [Minimum (cycles)]),
  [Page fault #math.arrow.r `SIGSEGV` handler], [2254],
  [`mprotect`], [2929],
  [Return from `SIGSEGV` handler], [902],
)

#pause

=== Indirect overhead

How to measure cache pollution?
