#import "@preview/touying:0.6.1": *
#import themes.simple: *

#show: simple-theme.with(
  config-info(
    title: [Paper summary: Extended User Interrupts (xUI): Fast and Flexible Notification without Polling],
    author: [Greg Depoire-\-Ferrer],
    date: datetime(
      year: 2025,
      month: 4,
      day: 17,
      hour: 14,
      minute: 0,
      second: 0,
    ),
    institution: [KrakOS],
  ),
)

#title-slide[
  #text(size: 1.5em)[
    Presentation of paper from ASPLOS ’25

    #text(
      weight: "bold",
      [Extended User Interrupts (xUI): Fast and Flexible Notification without Polling],
    )
  ]

  #let logo = rect(fill: black, image("krakos.png"), inset: .1em)

  #grid(
    columns: (auto, auto),
    column-gutter: 2em,
    [Thursday, 17 April 2025], link("https://www.liglab.fr/", logo),
  )
]

== Contributions

+ Deconstruction of *Intel's microarchitectural implementation* of user interrupts and *model of their timings*
+ *Enhancements* to Intel's user interrupts:
  - tracked interrupts
  - hardware safepoints
  - kernel bypass timer
  - interrupt forwarding
+ *Evaluation* of the enhancements in three workloads with a modified _gem5_ simulator

= Background

User-space notification mechanisms

== OS signals

*High overhead*: #quote(attribution: [The paper])[context switches] and
#quote(attribution: [The paper])[branch mispredictions and cache misses caused
  by contention with the kernel signal-handling code].

#quote(attribution: [The paper])[
  We measured the impact of signal delivery on some common benchmarks (e.g.,
  linpack2, base64 encoding) and found *each signal imposed roughly 2.4 μs of
  overhead at 2 GHz (without KPTI)*.
]

*Imprecise*: threads can be preempted anywhere.

== Some GCs need safepoints

#text(
  20pt,
  gray,
  quote(attribution: [The paper])[
    Signals are also imprecise, that is, they change control flow at arbitrary
    points in the receiver code. This is a problem for runtimes that rely on precise
    garbage collection (e.g., most Java and .NET implementations), as precise
    GCs rely on stackmaps. Stackmaps are metadata generated at compile time that
    tell the GC which stack elements are pointers—each map is valid only for a
    particular safepoint in program code. Thus, *if a thread is being preempted
    somewhere other than a safepoint, a GC cannot garbage collect it, as it has no
    way of identifying which objects that thread can reference*. In recent years,
    Go developed a unique solution to this problem by moving from precise GC to
    a hybrid approach that uses conservative GC for the most recent stack frame,
    and precise GC for the rest. Unfortunately, this is not a solution for most
    runtimes, as moving garbage collectors—which are quite popular—modify pointers
    on the stack. Thus, they cannot tolerate the imprecision of conservative GC.
  ],
)

== Intel's user interrupts

#text(23pt)[
  *Cheaper*: #quote(attribution: [The paper])[Receiving a notification with UIPI is roughly 3x-5x cheaper than signals—2800
    cycles vs. 600-900 cycles.]

  But still a pipeline flush on the receiver side #math.arrow.r growing overhead
  as the number of in-flight instructions in processors continues to increase.

  #table(
    columns: 5,
    table.header(
      [E2E Latency], [Receiver Cost], [`SENDUIPI`], [`CLUI`], [`STUI`]
    ),

    [1360 cycles], [720 cycles], [383 cycles], [2 cycles], [32 cycles],
  )

  *Imprecise*: threads can be preempted anywhere.

  *Narrow*: can only be triggered by software; no device interrupts or timer interrupts for now.
]

== Polling

*Precise*: #quote(attribution: [The paper])[total control over where polling occurs.]

*Low latency* and *high bandwidth*: #quote(attribution: [The paper])[
  When polling, each negative check is quite cheap (load with an L1 cache hit,
  and a correctly predicted branch), and each notification is still relatively
  cheap (load with an L2 cache miss, and a likely mispredicted branch).
]

*Inefficient* when waiting for notifications.

*Unscalable*: #quote(attribution: [The paper])[Polling also scales poorly as overheads increase linearly with the number of
  queues being polled, and each one burns a cache line.]

*Unpredictable*: requires the programmer to know when it's a good idea to poll.

#quote(attribution: [The paper])[
  Go did not preempt loops for its first ten years. Instead, it required
  developers to insert explicit calls to `runtime.Gosched()` (the scheduler)
  in loops when necessary. However, this was fragile, leading to hard-to-debug
  slowdowns and program freezes when overlooked. When the Go team explored
  mitigating this by adding checks to loops, they found it slowed down
  programs by a geomean of roughly 7%, and in the worst case, by up to 96%.
]

== Microarchitecture implementation of user interrupts

Steps for sending/receiving a user interrupt:
+ `senduipi` #math.arrow.r sender updates UPID.
+ Local APIC of sender sends interrupt to local APIC of receiver.
+ Local APIC of the receiver notifies the core by raising an interrupt signal line.
+ _Notification processing_ microcode procedure.
+ _User interrupt delivery_ microcode procedure.
+ The handler executes.
+ `uiret`.

= The xUI enhancements

== Tracked interrupts

- Flushing
- Draining
- _Tracking_

== Hardware safepoints

`safepoint` instruction or instruction prefix

What happens when a tracked interrupt is received, a safepoint is executed
speculatively but the speculation turns out to be wrong?

== Kernel bypass timer

New CPU instruction to start a periodic or one-shot timer. The timer sends a user interrupt to

== Interrupt forwarding

- Devices can send user interrupts

= Evaluation
