#set document(
  title: [Hardware-assisted user-space page fault handling],
  author: "Greg Depoire--Ferrer",
  date: datetime(year: 2025, month: 2, day: 24),
)

= Hardware-assisted user-space page fault handling

== User-space page fault handling

On Linux, page faults are currently handled in kernel space. Moving the
handling of page faults to user space allows for more flexibility in the page
reclaim policy to use, as well as more complicated swap storage backends.

One implementation of user space page fault handling is by using the
`userfaultfd` functionality in newer Linux kernels@userfaultfd-doc. It allows
registering a region with a userfault file descriptor that will be readable
whenever a thread encounters a page fault in that region. Reading from this FD
will return a structure with information about the page fault and an `ioctl`
can be made on the FD to install a PTE and resume the execution of the faulted
thread.

== User interrupts

_User interrupts_ is a feature that allows delivering interrupts directly
to user space@intel-manual. It was first introduced in Intel Sapphire Rapids
processors.

=== UPID

Threads that receive user interrupts need to have an _user posted-interrupt
descriptor_ or _UPID_ registered. The _UPID_ is managed by the OS. It is used
to by the `senduipi` instruction and during user-interrupt recognition and
delivery.

=== UITT

Threads that send user interrupt need to have a _user-interrupt target table_ or
_UITT_ registered. This table contains _UITT entries_ that contain a pointer to
a UPID as well as a user-interrupt vector. The `senduipi` instruction takes an
index into this table.

#bibliography("bibliography.yaml")
