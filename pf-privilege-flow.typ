#import "@preview/cetz:0.4.0"

#let pf-privilege-flow-diagram(bypass-kernel) = cetz.canvas({
  import cetz.draw: *

  content(
    (),
    box(stroke: 1pt, inset: .5em)[CPU],
    name: "cpu",
  )

  content(
    (rel: (5em, 0), to: "cpu.east"),
    anchor: "west",
    [Handler],
    name: "handler",
  )

  line(
    (rel: (0, 2.5em), to: ("cpu.east", 50%, "handler.west")),
    (rel: (0, -2.5em), to: ("cpu.east", 50%, "handler.west")),
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

