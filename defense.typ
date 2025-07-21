#import "@preview/touying:0.6.1": *
#import "./pf-privilege-flow.typ": pf-privilege-flow-diagram
#import themes.simple: *

#show: simple-theme.with(aspect-ratio: "16-9", config-info(
  title: [M2 Internship Defense],
  description: [Hardware-Assisted User-Space Page Fault Handling],
  author: "Greg Depoire--Ferrer",
  date: datetime(
    year: 2025,
    month: 7,
    day: 11,
    hour: 8,
    minute: 0,
    second: 0,
  ),
  institution: [ENS de Lyon],
))

#title-slide[
  = Hardware-Assisted User-Space Page Fault Handling

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
      #link("mailto:alain.tchana@grenoble-inp.fr")[Alain Tchana] \
      #link("mailto:renaud.lachaize@univ-grenoble-alpes.fr")[Renaud Lachaize]
    ],
  )

  #v(1em)

  #let logo = rect(fill: black, image("krakos.png"), inset: .1em)

  #grid(
    columns: (auto, auto),
    column-gutter: 2em,
    [February 27, 2025], logo,
  )

  #speaker-note[Talk about the LIG laboratory and explain a bit the title.]
]

== Monolithic kernels vs. microkernels

#speaker-note[
  First, let's start with a bit of background to explain what this internship is
  about.
]

A *monolithic kernel* is a big kernel that does many things.

#speaker-note[
  For example, it contains device drivers, filesystems drivers, memory
  allocation policies, scheduling, ...
]

#pause

Linux is a monolithic kernel.

#speaker-note[
  Linux has over 40 million lines of code. We are going to focus on Linux.
]

#pause

A *microkernel* is a small kernel with the minimum code so that most services
can be moved to user-space.

#speaker-note[Drivers and policies are pushed to user-space.]

== The limits of monolithic kernels

+ A single policy for all workloads.
+ Linux has a general purpose MM.
+ There is a diversity in applications and a diversity in hardware.

== The naive solution: modify Linux for each workload

+ *Not convenient*: Linux's codebase is very large
+ *Bugs* can be introduced and affect the whole system
+ *Not maintainable* as Linux's codebase changes quickly

// What about eBPF? The limitations of eBPF are ...

== Kernel bypass with a user-level manager

- CPU: ghOSt[Humphries '21], `sched_ext`
- Disk : Bento[Miller '21] First, let's start with a bit of background to
  explain what this internship is about.
- NIC: Snap[Marty '19], DPDK, Junction[Fried '24]
- #box(
    stroke: red,
    inset: .5em,
  )[Memory : USM[KrakOS], ExtMem[Jalalian '24]]

#speaker-note[ghOSt and Snap are from Google. I am working on the part in red.]

== The problem

#align(center + horizon, grid(
  columns: 2,
  column-gutter: 3cm,
  pf-privilege-flow-diagram(false), pf-privilege-flow-diagram(true),
))

#speaker-note[
  We want a fast mechanism to forward events to user-space. Current mechanisms
  are too slow because they involve the kernel.
]

== Problem statement

#speaker-note[
  In the report, there was an explanation about the flow of a page fault with
  userfaultfd, ExtMem and USM. Here, for simplicity we choose to focus on
  ExtMem.
]

After reading the code, we realized:
+ Some parts are duplicated.
+ Some could be omitted in most workloads.

We validate our hypotheses with benchmarks.

== Problem assessment

For all three current solutions, we notice important slowdowns.

*Solution*: UserFault (UF)

== UserFault (UF)

We drew inspiration from User Interrupts (UINTR).

#speaker-note[
  UINTR is a mechanism to send messages between applications without the kernel
  being involved.
]

Many use cases to support.

== Contributions

+ Hardware
+ Software

== Hardware contributions

+ I worked with a PhD student from Purdue univerity to brainstorm a new hardware
  interface.

#v(1fr)

(In progress.)

== Software contributions

#v(1fr)

(In progress.)

== Time spent

- $2/3$ of the internship spent on the problem assessment.
- $1/3$ of the internship spent on the design.

= Questions
