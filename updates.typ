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
      (rel: (0, -.5em), to: "kernel-space.south"),
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
      (rel: (0, -.5em), to: "user-space.south"),
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
      (rel: (0, -.5em), to: "page-fault-arrow"),
      angle: (rel: (0, -.5em), to: "page-fault-arrow.start"),
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
      (rel: (.5em, 0), to: "memory-manager-arrow"),
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
      (rel: (-.5em, 0), to: "return-arrow"),
      step(3, []),
      anchor: "east",
    )

    (pause,)

    line(
      (rel: (0, .5em), to: "memory-manager.east"),
      (rel: (0, .5em), to: "app.west"),
      mark: (end: ">"),
      name: "continue-arrow",
    )
    content(
      (rel: (0, .5em), to: "continue-arrow"),
      angle: (rel: (0, .5em), to: "continue-arrow.end"),
      step(4, [Continue execution]),
      anchor: "south",
    )
  }),
)
