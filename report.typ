#set document(
  title: [Hardware-assisted userspace page fault handling],
  author: "Greg Depoire--Ferrer",
  date: datetime(year: 2025, month: 2, day: 24),
)

= Hardware-assisted userspace page fault handling

== User interrupts

_User interrupts_ is a feature that allows delivering interrupts directly
to user space@intel-manual. It was first introduced in Intel Sapphire Rapids
processors.

=== UPID

Threads that receive user interrupts need to have an _user posted-interrupt
descriptor_ or _UPID_ registered. The _UPID_ is managed by the OS. It is used
to by the `senduipi` instruction and during user-interrupt recognition and
delivery.

#bibliography("bibliography.yaml")
