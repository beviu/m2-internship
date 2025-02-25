#import "@preview/touying:0.6.0": *
#import themes.simple: *
#import "@preview/cetz:0.3.2"

#set document(
  title: "Updates",
  author: "Greg Depoire--Ferrer",
  date: datetime(year: 2025, month: 2, day: 27),
)

#show: simple-theme.with(aspect-ratio: "16-9")

#let cetz-canvas = touying-reducer.with(
  reduce: cetz.canvas,
  cover: cetz.draw.hide.with(bounds: true),
)

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

    Now, policy is in user space.

    #pause

    === New latency costs

    - 2 context switches
    - 1 transition from kernel space to user space
    - 1 system call
  ],
)

== User interrupt page fault handling

#execution(
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
      user-space-statement[Do work on a different user thread],
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

TODO: sync version with `ioctl`

== User page fault check

Page fault generate user interrupts. The handler gets the virtual address of the access and installs
a new PTE using `io_uring` or an `ioctl`.

=== User-interrupt PTEs

- New PTE value that means that the CPU should send a user interrupt whenever an access is made.

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
