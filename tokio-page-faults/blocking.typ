#import "./simple-arrow.typ": simple-arrow

When a uthread $u_1$ blocks—such as due to a major page fault—would it be worth
it to have the kthread run another uthread $u_2$ instead until its time quantum
finishes instead of being suspended?

#let time-arrow = stack(
  dir: ltr,
  align(horizon, {
    let thickness = .12em
    let arrow-width = 4
    let y = thickness * arrow-width * .5
    simple-arrow(
      start: (0cm, y),
      end: (4.3cm, y),
      thickness: thickness,
      arrow-width: arrow-width,
    )
  }),
  h(.5em),
  [Time],
)

#let grayed-out = tiling(size: (4pt, 4pt), {
  let stroke = gray
  place(line(start: (0%, 100%), end: (100%, 0%), stroke: stroke + 2pt))
  place(line(start: (50%, 150%), end: (150%, 50%), stroke: stroke + 2pt))
  place(line(start: (-50%, 50%), end: (50%, -50%), stroke: stroke + 2pt))
})

#stack(
  grid(
    stroke: black,
    inset: 0pt,
    columns: (2cm, 2cm),
    rows: .75cm,
    align: center + horizon,
    grid.cell(fill: yellow, $u_1$),
    grid.cell(fill: grayed-out, text(.6em)[Scheduled \ Out]),
  ),
  .5em,
  time-arrow,
)

#stack(
  grid(
    stroke: black,
    inset: 0pt,
    columns: (2cm, 2cm),
    rows: .75cm,
    align: center + horizon,
    grid.cell(fill: yellow, $u_1$),
    grid.cell(fill: green, $u_2$),
  ),
  .5em,
  time-arrow,
)

We want to measure how much of their time quantum kthreads lose because they
are running a uthread that blocks.

If the scheduler is fair, we can hope that it will give back this part to the
kthread later. However, latency still increased. Additionally, if the system is
not CPU bound, and there are as many kthreads as there are CPUs, then the CPU
was idle for some time when it could have been executing a uthread.
