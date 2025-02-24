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

#let step(i, body) = grid(
  columns: (auto, auto),
  align: horizon,
  column-gutter: .25em,
  circle(
    inset: .1em,
    align(center + horizon, text(.75em, str(i))),
  ),
  body,
)

#align(
  center + horizon,
  cetz-canvas({
    import cetz.draw: *

    rect(
      (0, 0),
      (10, 8),
      fill: red.transparentize(75%),
      stroke: none,
      name: "kernel-space",
    )
    content(
      (rel: (0, -1em), to: "kernel-space.south"),
      text(red.darken(50%), [Kernel space]),
      anchor: "north",
    )

    content(
      ("kernel-space.north", 30%, "kernel-space.south"),
      padding: .5em,
      frame: "rect",
      stroke: red.darken(50%),
      text(red.darken(50%), [Memory manager]),
      name: "memory-manager",
    )

    content(
      ("kernel-space.north", 70%, "kernel-space.south"),
      padding: .5em,
      frame: "rect",
      stroke: red.darken(50%),
      text(red.darken(50%), [Interrupt handler]),
      name: "interrupt-handler",
    )

    rect(
      (10, 0),
      (20, 8),
      fill: blue.transparentize(75%),
      stroke: none,
      name: "user-space",
    )
    content(
      (rel: (0, -1em), to: "user-space.south"),
      text(blue.darken(50%), [User space]),
      anchor: "north",
    )

    content(
      "user-space",
      padding: .5em,
      frame: "rect",
      stroke: blue.darken(50%),
      text(blue.darken(50%), [App]),
      name: "app",
    )

    line(
      (rel: (0, -.5em), to: "app.west"),
      (rel: (0, -.5em), to: "interrupt-handler.east"),
      mark: (end: ">"),
      name: "page-fault-arrow",
    )
    content(
      "page-fault-arrow",
      angle: "page-fault-arrow.start",
      padding: (top: .5em),
      step(1, [Page fault]),
      anchor: "north",
    )

    (pause,)

    line(
      (rel: (.5em, 0), to: "interrupt-handler.north"),
      (rel: (.5em, 0), to: "memory-manager.south"),
      mark: (end: ">"),
      name: "memory-manager-arrow",
    )
    content(
      "memory-manager-arrow",
      padding: (left: .5em),
      step(2, []),
      anchor: "west",
    )

    (pause,)

    line(
      (rel: (-.5em, 0), to: "memory-manager.south"),
      (rel: (-.5em, 0), to: "interrupt-handler.north"),
      mark: (end: ">"),
      name: "return-arrow",
    )
    content(
      "return-arrow",
      padding: (right: .5em),
      step(3, []),
      anchor: "east",
    )

    (pause,)

    line(
      (rel: (0, .5em), to: "interrupt-handler.east"),
      (rel: (0, .5em), to: "app.west"),
      mark: (end: ">"),
      name: "resume-arrow",
    )
    content(
      "resume-arrow",
      angle: "resume-arrow.end",
      padding: (bottom: .5em),
      step(4, [Resumes]),
      anchor: "south",
    )
  }),
)

== User space page fault handling

#align(
  center + horizon,
  cetz.canvas({
    import cetz.draw: *

    rect(
      (0, 0),
      (10, 8),
      fill: red.transparentize(75%),
      stroke: none,
      name: "kernel-space",
    )
    content(
      (rel: (0, -.5em), to: "kernel-space.south"),
      text(red.darken(50%), [Kernel space]),
      anchor: "north",
    )

    content(
      ("kernel-space.north", 30%, "kernel-space.south"),
      padding: .5em,
      frame: "rect",
      stroke: red.darken(50%),
      text(red.darken(50%), [USM kernel module]),
      name: "usm-kernel-module",
    )

    content(
      ("kernel-space.north", 70%, "kernel-space.south"),
      padding: .5em,
      frame: "rect",
      stroke: red.darken(50%),
      text(red.darken(50%), [Interrupt handler]),
      name: "interrupt-handler",
    )

    rect(
      (10, 0),
      (20, 8),
      fill: blue.transparentize(75%),
      stroke: none,
      name: "user-space",
    )
    content(
      (rel: (0, -.5em), to: "user-space.south"),
      text(blue.darken(50%), [User space]),
      anchor: "north",
    )

    content(
      ("user-space.north", 30%, "user-space.south"),
      padding: .5em,
      frame: "rect",
      stroke: blue.darken(50%),
      text(blue.darken(50%), [Memory manager]),
      name: "memory-manager",
    )

    content(
      ("user-space.north", 70%, "user-space.south"),
      padding: .5em,
      frame: "rect",
      stroke: blue.darken(50%),
      text(blue.darken(50%), [App]),
      name: "app",
    )

    line(
      "app",
      "interrupt-handler",
      mark: (end: ">"),
      name: "page-fault-arrow",
    )
    content(
      "page-fault-arrow",
      angle: "page-fault-arrow.start",
      padding: (top: .5em),
      step(1, [Page fault]),
      anchor: "north",
    )

    line(
      "interrupt-handler",
      "memory-manager",
      stroke: (dash: "dashed"),
      mark: (end: ">"),
      name: "memory-manager-arrow",
    )
    content(
      "memory-manager-arrow",
      angle: "memory-manager-arrow.end",
      padding: (top: .5em),
      step(2, [Notifies]),
      anchor: "north",
    )

    line(
      "memory-manager",
      "usm-kernel-module",
      mark: (end: ">"),
      name: "system-call-arrow",
    )
    content(
      "system-call-arrow",
      padding: (bottom: .5em),
      step(3, [System call]),
      anchor: "south",
    )

    line(
      "usm-kernel-module",
      "app",
      mark: (end: ">"),
      name: "resume-arrow",
    )
    content(
      "resume-arrow",
      angle: "resume-arrow.end",
      padding: (bottom: .5em),
      step(4, [Resumes]),
      anchor: "south",
    )
  }),
)

== User interrupt page fault handling

- App triggers a page fault.
- User interrupt handler is called.
- A call is done to the USM kernel module to install the new PTE.
- The user space scheduler resumes the user thread.

== Ideas

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
