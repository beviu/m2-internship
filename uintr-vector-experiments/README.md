# User interrupt vector experiments

## Prerequisites

Compile the test program:

```sh
clang run-uintr-vector-experiment.c -o run-uintr-vector-experiment -muintr
```

Apply the kernel patches onto Intel's `uintr-next` branch
(commit `0ee776bd38532358159013ed0188693b34c46cf5` at
https://github.com/intel/uintr-linux-kernel.git) and boot on the patched kernel.

## UINV is `X86_TRAP_PF`

What happens if the UINV is set to `X86_TRAP_PF` and we do a page fault? Can we
turn page faults into user interrupts?

```sh
./run-uintr-vector-experiment --uinv=14 --set-upir --no-self-ipi --page-fault --spin=1
```

No, it doesn't work: the user interrupt handler is not called.

## UINV is another IRQ vector number

What about other IRQ vector numbers? For each vector number, let's try setting
the UINV to it, waiting for 10 seconds and checking if we've received a user
interrupt.

```bash
for ((vector = 0; vector < 256; vector++)); do
  echo -en "$vector\t"
  ./run-uintr-vector-experiment --uinv=$vector --set-upir --no-self-ipi --spin=10
done
```

I did a single run. No user interrupts were received except for IRQ vector
number 234 (`LOCAL_TIMER_VECTOR` in Linux) which received a single user
interrupt.

## NV in UPID is set to another IRQ vector number

What happens if we change the NV field in the UPID to make `senduipi` send an
IRQ with a different vector number?

```bash
for ((vector = 0; vector < 256; vector++)); do
  echo -en "$vector\t"
  ./run-uintr-vector-experiment --uinv=$vector --upid-nv=$vector --senduipi --no-self-ipi --spin=1
done
```

From 0 to 31, no user interrupt was generated. From 32 to 255, a user interrupt
was received.
