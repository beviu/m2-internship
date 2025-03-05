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

== Measuring the execution time of small steps

In order to evaluate the hardware modifications that we propose, we make a model
of the current page fault handling and then use it to approximate what would be
the execution time of a page fault with the hardware modifications.

To do that, we will need to measure the execution time of several small steps
during page fault handling.

We start with the normal Linux page fault handling in the case of a minor page
fault. We are interested in the execution time in cycles of the following steps:

- Time from execution of faulting instruction to first instruction in OS
  exception handler.
- Time to save the CPU state (including registers) before the C code is called.
- Time to search for the VMA.
- Time to find a free physical page.
- Time to update the PTE.
- TIme to restore the CPU state after the C code returns.

We will instrument the kernel to measure these times.

However, we only want to instrument the code flow for a single process. To do
this, we are going to add checks that rely on the `current` pointer which is a
pointer to the task that is currently executing on the CPU.

=== Results

I ran my `detailed-page-fault` benchmark inside a virtual machine on my computer
with a _Intel Core i5-1335U_ processor and Linux 6.14, and got the following
results:

#align(
  center,
  table(
    columns: (auto, auto),
    table.header([], [Minimum (cycles)]),
    [Exception], [479],
    [Save state], [303],
    [Search for VMA], [1448],
    [Handle fault], [308],
    [Restore state], [61],
    [`iret`], [257],
  ),
)

#bibliography("bibliography.yaml")
