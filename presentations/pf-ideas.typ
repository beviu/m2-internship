#import "@preview/touying:0.6.1": *
#import themes.simple: *
#import "../execution.typ": *

#show: simple-theme.with(config-info(author: [Greg Depoire-\-Ferrer]))

== Idea 1: tell USM in advance

#grid(
  columns: (auto, 1fr, auto),
  text(
    10pt,
    execution(
      (
        [Faulting thread],
        user-stmt[Faulting instruction],
        internal-stmt[CPU exception],
        kernel-stmt[Push registers and switch CR3],
        kernel-stmt[Find VMA],
        kernel-stmt[Walk page table],
        kernel-stmt[Notify USM thread],
        (set-event: "usm-called"),
        (wait-for-event: "usm-returned"),
        kernel-stmt[Read PFN],
        kernel-stmt[Switch back CR3 and pop registers],
        kernel-stmt[`iret`],
      ),
      (
        [USM thread],
        (wait-for-event: "usm-called"),
        user-stmt[Handle fault],
        (set-event: "usm-returned"),
      ),
    ),
  ),
  text(25pt, align(center + horizon, math.arrow)),
  text(
    10pt,
    execution(
      (
        [Faulting thread],
        user-stmt[Faulting instruction],
        internal-stmt[CPU exception],
        kernel-stmt[Notify USM thread],
        (set-event: "usm-called2"),
        kernel-stmt[Push registers and switch CR3],
        kernel-stmt[Find VMA],
        kernel-stmt[Walk page table],
        kernel-stmt[Read PFN],
        kernel-stmt[Switch back CR3 and pop registers],
        kernel-stmt[`iret`],
      ),
      (
        [USM thread],
        (wait-for-event: "usm-called2"),
        user-stmt[Handle fault],
      ),
    ),
  ),
)

== Idea 2:
