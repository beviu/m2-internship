CC=gcc

all:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DDEBUG -DSpDEBUG -DUTHREAD -g -lm
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DDEBUG -DSpDEBUG -DUTHREAD -g

timer:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DDEBUG -DSpDEBUG -DUTHREAD -g -lm -DTSERV
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DDEBUG -DSpDEBUG -DUTHREAD -g -DTSERV

uintr:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DDEBUG -DSpDEBUG -DUTHREAD -g -lm -DUINTR -muintr
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DDEBUG -DSpDEBUG -DUINTR -DUTHREAD -g -muintr

iuthread:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DDEBUG -DSpDEBUG -DUTHREAD -g -lm -DUINTR -muintr -DIUTHREAD
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DDEBUG -DSpDEBUG -DUINTR -DUTHREAD -g -muintr -DIUTHREAD				# upcoming IU usage..

iuthreadnodebug:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DUTHREAD -g -lm -DUINTR -muintr -DIUTHREAD
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DUINTR -DUTHREAD -g -muintr -DIUTHREAD

iuthreadnouintr:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DDEBUG -DSpDEBUG -DUTHREAD -g -lm -DIUTHREAD
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DDEBUG -DSpDEBUG -DUTHREAD -g -DIUTHREAD

iuthreadnouintrnodebug:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DUTHREAD -g -lm -DIUTHREAD
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DUTHREAD -g -DIUTHREAD

spemul:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DDEBUG -DSpDEBUG -DUTHREAD -g -lm -DUINTR -muintr -DSpEMUL
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DDEBUG -DSpDEBUG -DUINTR -DUTHREAD -g -muintr

spemulnodebug:		# more cases to ev. do..
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DUTHREAD -g -lm -DUINTR -muintr -DSpEMUL
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DUINTR -DUTHREAD -g -muintr

uintrnostop:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DDEBUG -DSpDEBUG -DUTHREAD -g -lm -DUINTR -muintr -DNSTOP
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DDEBUG -DSpDEBUG -DUINTR -DUTHREAD -g -muintr

uintrtimer:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DDEBUG -DSpDEBUG -DUTHREAD -g -lm -DUINTR -muintr -DTSERV
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DDEBUG -DSpDEBUG -DUINTR -DUTHREAD -g -muintr -DTSERV

uintrtimernodebug:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DUTHREAD -lm -DUINTR -muintr -DTSERV
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DUINTR -DUTHREAD -muintr -DTSERV

uintrnodebug:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DUTHREAD -lm -DUINTR -muintr
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DUINTR -DUTHREAD -muintr

uintrnodebugnosleep:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DUTHREAD -lm -DUINTR -muintr -DNOSLEEP

uintrnouthread:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DDEBUG -DSpDEBUG -g -lm -DUINTR -muintr
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DDEBUG -DSpDEBUG -DUINTR -g -muintr

uintrnouthreadnd:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -g -lm -DUINTR -muintr
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DUINTR -g -muintr

nouthread:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DDEBUG -DSpDEBUG -g -fsanitize=address -fno-omit-frame-pointer -lasan -lm
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DDEBUG -DSpDEBUG -g

nodebug:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DUTHREAD -lm
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DUTHREAD

nosleep:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DDEBUG -DSpDEBUG -DUTHREAD -g -lm -DNOSLEEP

nosleepnodebug:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DUTHREAD -lm -DNOSLEEP

timernodebug:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -DUTHREAD -lm -DTSERV
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DUTHREAD -DTSERV

nouthreadnodebug:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -lm
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread

nouthreadtimernodebug:
	$(CC) $(CFLAGS) examples/project-2/alloc/src/policiesSet1.c examples/project-2/evict/src/policiesSet1.c impl/usm/usm.c impl/usm/hashmap.c impl/usm/sched.c impl/usm/list_sort.c impl/com/com.c impl/com/event.c impl/policies/alloc/alloc.c impl/policies/evict/swap.c examples/project-2/project-2.c -o project-2 -lpthread -lm -DTSERV
	$(CC) $(CFLAGS) impl/com/waker.c -o usmWaker -lpthread -DTSERV

launchnowaker:
	sudo modprobe usm_lkmRM
	sudo LD_PRELOAD=./swapcontext_Fast.S ./project-2 examples/project-2/cfg/alloc_policy_assignment_strategy.cfg

launch:
	sudo modprobe usm_lkmRM
	sudo LD_PRELOAD=./swapcontext_Fast.S ./project-2 examples/project-2/cfg/alloc_policy_assignment_strategy.cfg &
	sudo ./usmWaker

# some more launch options..
