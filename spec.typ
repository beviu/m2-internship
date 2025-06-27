#import "@preview/bytefield:0.0.7": *

= User faults (UF)

== Usecase

An application wants to bypass the kernel and directly handle page faults in user space.

=== Coexistence with Linux's page fault handler

The application might want to handle only a subset of the page faults and let
Linux handle the rest as usual. For example, a database could let Linux handle
page faults on the stack and executable mappings but handle page faults on the
database file itself.

The application might want to specify which page fault should be handled by the
kernel versus by itself in the following ways:

+ A range of virtual addresses on which every fault will become a user fault.
+ A `mmap` flag that makes every fault on the memory mapping become a user fault.
+ The application might ask for every page fault on the heap to be a user fault.
+ The application might ask for every page fault the userspace part of the
  address space to be a user fault.
+ The application might ask for every page fault happening inside a code
  section to become a user fault.
+ A flag or attribute of a thread that indicates if every fault becomes a user
  fault.
+ A mix of the previous ideas.

== Interface

```c
int main() {
  uf_register_handler(handler);
  uf_enable();
  // ...
  uf_disable();
}
```

= Implementation

- The address of the handler is stored in a MSR.
- `uf_enable`/`uf_disable` modify a flag in an MSR.

== User fault handler

The user fault handler has the same ABI as a user interrupt handler, except for
the vector number which is replaced by an _error code_ which encodes information
about the fault:

#bytefield(
  msb: right,
  bitheader("bounds"),
  flag("Pres."),
  flag("Write"),
  flag("User"),
  flag("Res."),
  flag("Fetch"),
)

== New instructions

=== `stuf`

*Opcode*: `F3 0F 01 EB`

Enables user faults.

=== `cluf`

*Opcode*: `F3 0F 01 EA`

Disables user faults.

=== `rdaddr`

*Opcode*: `0F 01 D2`

Reads the user fault virtual address into `rax`.

=== `kret`

*Opcode*: `F3 0F 01 E9`

= Questions

Should user interrupts and UF be able to coexist?
