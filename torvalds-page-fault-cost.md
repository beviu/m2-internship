One of the things I end up doing is do a lot of performance profiling on core
kernel code, particularly the VM and filesystem.

And I tend to do it for the "good case" - when things are pretty much perfectly
cached.  Because while I do care about IO, the loads I personally run tend to
be things that cache well. For example, one of my main loads tends to be to do
a full kernel build after most of the pulls I do, and it matters deeply to me
how long that takes, because I don't want to do another pull until I've
verified that the first one passes that basic sanity test.

Now, the kernel build system is actually pretty smart, so for a lot of driver
and architecture pulls that didn't change some core header file, that
"recompile the whole kernel" doesn't actually do a lot of building: most of
what it does is check "ok, that file and the headers it depends on hasn't
changed, so nothing to do". 

But it does that for thousands of header files, and tens of thousands of C
files, so it all does take a while. Even a fully built kernel ("allmodconfig",
so a pretty full build) takes about half a minute on my normal desktop to say
"I'm done, that pull changed nothing I could compile".

Ok, so half a minute for an allmodconfig build isn't really all that much, but
it's long enough that I end up waiting for it before I can do the next pull,
and short enough that I can't just go take a coffee break.

Annoying, in other words.

So I profile that sh*t to death, and while about half of it is just "make"
being slow, this is actually one of the few very kernel-intensive loads I see,
because it's doing a *lot* of pathname lookups and does a fair amount of small
short-lived processes (small shell scripts, "make" just doing fork/exit, etc).

The main issue used to be the VFS pathname lookup, and that's still a big deal,
but it's no longer the single most noticeable one.

Most noticeable single cost? Page fault handling by the CPU.

And I really mean that "by the CPU" part. The *kernel* VM does really well.
It's literally the cost of the page fault itself, and (to a smaller degree) the
cost of the "iret" returning from the page fault.

I wrote a small test-program to pinpoint this more exactly, and it's
interesting. On my Haswell CPU, the cost of a single page fault seems to be
about 715 cycles. The "iret" to return is 330 cycles. So just the page fault
and return is about 1050 cycles. That cost might be off by some small amount,
but it's close. On another test case, I got a number that was in the 1150 cycle
range, but that had more noise, so 1050 seems to be the *minimum* cost.

Why is that interesting? It's interesting, because the kernel software overhead
for looking up the page and putting it into the page tables is actually *much*
lower. In my worst-case situation (admittedly a pretty made up case where we
just end up mapping the fixed zero-page), those 1050 cycles is actually 80.7%
of all the CPU time. That's the extreme case where neither kernel nor user
space does much anything else that fault pages, but on my actual kernel build,
it's still 5% of all CPU time.

On an older 32-bit Core Duo, my test program says that the page fault overhead
is "just" 58% instead of 80%, and it does seem to be because page faults have
gotten slower (the cost on Core Duo seems to be "just" 700 + 240 cycles).

Another part of it is probably because Haswell is better at normal code (so the
fault overhead is relatively more noticeable), but it was sad to see how this
cost is going in the wrong direction.

I'm talking to some Intel engineers, trying to see if this can be improved.
