#import "@preview/subpar:0.2.2"
#import "./simple-arrow.typ": simple-arrow

= Asynchronous page faults (and system calls)

One way to see a page fault is to see it as a sort of system call to the
operating system's memory management code: it switches the CPU to kernel-mode,
jumps to a predefined handler in the kernel, the kernel does some processing,
and then returns to user-mode. Like system calls#footnote[This is true even for
  "non-blocking" system calls or system calls that allow multiplexing operations
  such as `io_uring_enter`.], page faults are synchronous: during the handling
of the page fault, no user code can run.

Some applications have a notion of user-level threads (_uthreads_), _green
threads_ or _tasks_. They are units of work that can run concurrently. They are
multiplexed on top of the kernel-level threads (_kthreads_)—the "real" threads,
as the kernel sees them.

Consider a kthread that is executing a uthread $u_1$. When $u_1$ triggers a page
fault (or does a system call), would it be worth it to have the kthread switch
to another uthread $u_2$ and continue execution instead of being suspended?

== Minor page faults (and non-blocking system calls)

*TBD*

== Major page faults (and blocking system calls)

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

#let time-diagram(body) = stack(
  align(
    left,
    body,
  ),
  .5em,
  time-arrow,
)

#let grayed-out = tiling(size: (4pt, 4pt), {
  let stroke = gray
  place(line(start: (0%, 100%), end: (100%, 0%), stroke: stroke + 2pt))
  place(line(start: (50%, 150%), end: (150%, 50%), stroke: stroke + 2pt))
  place(line(start: (-50%, 50%), end: (50%, -50%), stroke: stroke + 2pt))
})

#subpar.grid(
  columns: 2,
  caption: [Two ways to handle a uthread that blocks],
  figure(caption: [The kthread blocks], time-diagram(grid(
    stroke: black,
    inset: 0pt,
    columns: (2cm, 2cm),
    rows: .75cm,
    gutter: 0pt,
    align: center + horizon,
    grid.cell(fill: yellow, $u_1$),
    grid.cell(fill: grayed-out, text(.6em)[Scheduled \ Out]),
  ))),
  figure(caption: [The kthread switches to another uthread], time-diagram(grid(
    stroke: black,
    inset: 0pt,
    columns: (2cm, 2cm),
    rows: .75cm,
    gutter: 0pt,
    align: center + horizon,
    grid.cell(fill: yellow, $u_1$),
    grid.cell(fill: green, $u_2$),
  ))),
)

To understand the performance implications of the two ways of handling a uthread
that blocks, let's look at two extreme cases:
- when the system is _CPU bound_, and
- when the system is _otherwise idle_.

=== When the system is CPU bound

In this case, the system is CPU bound and there are many other kthreads waiting
to run. When our kthread blocks, the scheduler is going to pick another kthread
$k_2$ and switch to it.

#figure(caption: [The scheduler switches to the kthread $k_2$], time-diagram(
  grid(
    stroke: black,
    inset: 0pt,
    columns: (2cm, 2cm),
    rows: .75cm,
    gutter: 0pt,
    align: center + horizon,
    grid.cell(fill: yellow, $u_1$),
    grid.cell(fill: purple, $k_2$),
  ),
))

If the scheduler is fair, it will return the remaining time in $k_1$'s time
quantum to $k_1$ later. However, latency still increased for $k_1$ and the
uthreads that it is executing. Additionally, the cost of a kthread context
switch has been paid.

=== When the system is otherwise idle

In this case, the system is otherwise idle, there are no other kthreads to run
and the application has as many kthreads as there are CPUs.

#figure(caption: [The CPU is idle], time-diagram(grid(
  stroke: black,
  inset: 0pt,
  columns: (2cm, 2cm),
  rows: .75cm,
  gutter: 0pt,
  align: center + horizon,
  grid.cell(fill: yellow, $u_1$),
  grid.cell(fill: grayed-out, text(.6em)[Scheduled \ Out]),
)))

The scheduler puts the CPU to sleep. The latency of $k_1$ and the uthreads that
it is executing—except for $u_1$—increased. Additionally, the cost of putting
the CPU to sleep and waiting for it to resume is paid.

=== The number of page faults inside the application



=== The proportion of time lost

We will focus on the case where the system is otherwise idle, because in this
case it is easy to see that the current behavior is not what we want.

We would like to take an application and measure:
- the time $t_"run"$ that it spends running in a kthread in user-space, and
- the time $t_"lost"$ that is lost scheduling out the kthread, waiting for it to
  become _runnable_ again, and scheduling back to it again.

Then we could compute the total time $t_"total" = t_"run" + t_"lost"$ and
compute the proportion of the total time that we lost $x_"lost" = t_"lost"
/ t_"total"$.

The question is how do we measure $t_"lost"$?
