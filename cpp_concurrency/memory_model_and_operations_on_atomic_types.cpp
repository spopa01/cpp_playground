/*

C++ Concurrency in Action

One of the most important features of C++11 Standard is the new (conurrency aware) memory model.

There are two impoertant aspects to the memory model: the basic structural aspects (which relate to how things are laid out in memory) and then the concurrency aspect.

From the basic structural aspects, there are four important thinks to understand:
- every variable is an object ( now don't tkink about java/C# objects =)) ), including those that are members of other objects.
- every object occupies at least one memory location (so is important to point out for zero bitfields...)
- variables of fundamental type such as int or char etc ... are exactly one memory location, whatever their size, even if they're adjancent or part of an array.
- adjancent bit fields are part of the same memory mocation (see also observartion 2 for zero bitfields).

The crucial aspects for concurrency:
- everything is based on these memory locations
- if two threads access separate memory locations there is no problem
- if two threads access same memory location but just for reading, again there is no problem (read-only does not require protection or synchronization)
- if either thread is modifying the data, there is a potential race condition.

In order to avoid race conditions, there has to be an enforced ordering between accesses in the two threads.
There are two ways two main ways of doing this:
- one way to impose ordering is to use mutexes (previously described)
- the other way is to use the synchronization properties of atomic operations in order to enforce it.

NOTE: In the second case, although the undefined behivour is removed, the race itself is not removed, the order in which the two atomic operations touch the meory remains undefined.
      ( but after each atomic memory operation the memory location remains consistent, so the next atomic memory operation will see the memoru location in the final/consistent state... )

The atomic type are all accessed through a specialization of the std::atomic<> for the basic types (eg. int, char, unsigned int etc plus specializations for the pointer type)
There is also a special std::atomic_flag (which is the only one guaranteed/required bt the C++ Standard to be always lock_free). An atomic flag is initialized to clear, and then it can be either 
queried and set (test_and_set) or cleared (clear).

This is all you have to know if you avoid relaxed atomics =))  (Herb Sutter) and you should try to do so...

Going deep:
***********************************************************************************************************
***** http://channel9.msdn.com/Shows/Going+Deep/Cpp-and-Beyond-2012-Herb-Sutter-atomic-Weapons-1-of-2 *****
***** http://channel9.msdn.com/Shows/Going+Deep/Cpp-and-Beyond-2012-Herb-Sutter-atomic-Weapons-2-of-2 *****
***********************************************************************************************************

**** Optimizations, Races and the Memory Model:

The von Neumann machine are an utopia today, especially with the appearance of modern multicore machines which have more circuits for L1 & L2 & L3 - Data & Instructions than for actual computation.
Also, already for a long while we have store-buffers because writes are much more expensive than reads and you really want to buffer them...

!!!! Key feature and assumption : the CPUs maintain fully coherent caches...

The Talk in one slide: Don't write a race condition or use non-default atomics and your code will do what you think...

The Truth: Does your computer execute the program you wrote? ->  NO -> Think Compiler optimizations, processor OoO execution, cache coherency etc... 

Two Key Concepts:
- Sequential consistency (SC): Executing the program you wrote ... Seems great
- Race condition: A memory location (variable) can be simultaneously accessed by two threads, and at least one thread is a writer (and no synchronization).
      Memory location == non-bitfield variable or sequence of non-zero-length bitfield variables
      Simultaneously == without happens-before order

In reality : Transformations = Reorder + Invent + Remove
  Source Code -> [ Compiler (subexpr elimination, reg allocation, Software Transactiona Memory(STM)) -> Processor (prefetch, speculation, overlap writes) -> Cache(store buffers, shared caches...) ] -> Actual Execution

AND:
- you can't really tell at which level the transformation happens
- you only care about the fact that "your correctly synchronized program" behaves as if:
      - memory ops are actually executed in an order that appears equivalent to some "sequentially consistent interleaved exevution" of the memory ops of each thread in your source code
      - including that each write appears to be atomic and globally visible simultaneously to all processors

BUT: Transformations at all levels are equivalent => can reason about all transformations as "reorderings of source code loads and stores"

A Bit of History: Deckker's and Peterson's Algorithms
- consider flags are shared and atomic but unordered, initially zero:
    Thread1                                   Thread2
    flag1 = 1;      //a: declare intent       flag2 = 1;      //c: declare intent
    if(flag2 != 0)  //b                       if(flag11 != 0) //d  
      //resolve contention                      //resolve contention
    else                                      else
      //enter critical section                  //enter critical section

Could both threads enter the critical section? -> Maybe -> If a can pass b or c can pass d this breaks (with a little bit of help from Independent Reads of Independent Writes (IRIW)...)
Solution1: use suilable atomic type for flag variables (good)
Solution2: use system locks instead of rolling your own (good)
Solution3: write a memory barrier after a and c (problematic)

Few examples of legal single-threaded optimizations, like: cut unused writes, use registers for loops, etc... read/writes reorderings

Optimizations:
What the compilers know:
  - all memory operations in THIS THREAD and exactly what they do, including data dependencies.
  - how to be conservative enough in the face of possible aliasing
What the compiler doesn't know:
  - which memory locations are "mutable shared" variables and could change asynchronously due to memory operations IN ANOTHER THREAD
  - how to be conservative enough in the face of possible sharing
Soulution: Tell it, so somehow identify the operations on "mutable shared" locations...

Software Mememory Models have converged on SC for data-race-free programs (SC-DRF): Java required since 2005 / C11 and C++11: SC-DFR default (relaxed == transitional tool).
*********************************************
Memory Models are contracts which say: if you promise to correctly synchronize your program (no race conditions) -> the system promises to provide the illusion of executing the "program you wrote", isn't that cool =))
*********************************************

**** Ordering - What are Aquire and Release:

Key general concept: Transactions
Transaction = logical operation on related data that maintain an invariant.
They are:
  Atomic: all-or-nothing
  Consistent: Reads a consistent state, or takes data from one consistent state to another
  Independent: Correct in the presence of other transactions on the same data.
(Example: the very classic back account problem - don't expose inconsistent states - eg: credit without also debit)

Key general concept: Critical Region
Critical Region = code that must execute in isolation  (used to implement transactions)
And it can be expressed using:
- locks
  { lock_guard<mutex> hold(mtx);        //enter critical section
    ...read/write x...
  }                                     //exit critical section
- ordered atomics (whose_turn is std::atomic variable protecting x):
  while( whose_turn != me ){}           //enter critical region (atomic read "aquires" value) - atomic read is equivalent to aquire operation
    ...read/write x...
  whose_turn = someone_else;            //exit critical section                                - atomic write is equivalent to release operation
- transactional memory ... still research (but not far from mutex/spin-lock as the way you use it)

!!!!!!!!! Key RULE : Code Can't Move Out (from the critical region)
      - it is ILLEGAL for a system to transfer this: mtx.lock(); x = 100; mtx.unlock();
      - into this:    x = 100; mtx.lock(); mtx.unlock();   OR this:   mtx.lock(); mtx.unlock(); x = 100;

      BUT: Code CAN Safely Move In (from before or after the transaction) the critical section...

Key Concept: Aquire and Release
  A release store makes "its prior accesses visible" to a thread performing an acquire load "that sees (pairs with) that store"
  -One-way barriers: An acquire barrier and a release barrier...
  -These are fundamental hardware and software concepts

Good Fences (make good neighbours): (Acquire-Release) vs (Full Fence - Usually hapens around CAS operations)

As a side note...
"Plain" Acquire/Release vs. SC Acquire/Release -> Plain Acquire/Release allows release-acquire (not acquire-release) to pass each other this is the reason on some (older) architectures is necessary an extra fance.

FACT of Life: Memory synchronization actively works against important modern hardware optimizations => want to do as little as possible... We always want to keep memory pipeline full.

How do modern processors cope with latancy ? => Add Concurrency Everywhere...
  - Parallelize (leverage computer power): 
      - Pipeline, execute out of order (OoO): launch expensive memory operations earlier, and do other work while waiting...
      - Add hardware threads: have other work available for the same CPU core to perform while other work is blocked in memory
  - Cache (leverage capacity):
      - Add instruction caches
      - Add multiple levels of data chaches (unit of sharing = cache line)
      - Other Buffers, like store buffering, because writes are usually more expensive
  - Speculate (leverage bandwidth, compute):
      - Predict branches: guess whether an "if" will be true
      - Other optimistic execution, like execution (e.g try both branches)
      - Prefetch, scout (warm up the cache)

The more we interrupt this flow the less performance we see...

**** Ordering - How are Mutexes, Atomics and/or Fences Implemented:

Automatic Acquire and Release
  - don't write fences by hand
  - do make the compiler write barriers for you by using "critical region"

For example on a IA64:

  mtx.lock();                   //acquire mtx => ld.acq mtx
  ...read/write x...  
  mtx.unlock();                 //release mtx => st.rel mtx

  std::atomics read = acquire, write = release
  while( whise_turn != me ) {}  //read whose_turn  => ld.acq whose_turn
  ...read/write x...
  whose_turn = someone_else;    //write whose_turn => sr.rel whose_turn

SC > Acq/Rel Alone: Some examples:
  
  - Transitivity/casuality : x and y are std::atomic, are variables initially zero.
    Thread1     Thread2       Thread3
    g=1         if(x==1)      if(y==1)
    x=1           y=1           assert(g==1);
    *It must be impossible for assertion to fail - wouldn't be SC
  - Total store oreder: x and y are std::atomic and initially zero.
    Thread1     Thread2       Thread3               Thread4
    x=1         y=1           if(x==1 && y==0)      if(y==1 && x==0)
                                print("x first")      print("y first")
    *It should be impossible to print both messages - wouldn't SC

Controlling Reordering: Mutexes
  - Use mutex locks to protect code that reads/writes shared variables
  - Advantages: locks acquire/release induce ordering and nearly all reordering/invention/removal weirdness just goes away
  - Disadvantages: requires care on every use of the shared variables (deadlocks/livelocks)
Controlling Reordering: Atomics (std::atomics)
  - Atomic types are automatically safe from reordering
  - Advantages: just tag the variable, not every plcae it's used
  - Disadvantages: Writing correct atomics cpde is harder than it looks
Controlling Reordering: (manual) Fences & ordered apis
  - simply avoid... most of the time they are suboptimal

Ordered Atomics:
  - Java and .NET volatile -> Always SC
  - C++11 atomic<T>, C11 atomic_* -> DEFAULT SC
  - Semantics and operations:
    - each individual read/write is atomic (no torn reads, no locking needed)
    - each thread's reads/writes are guaranteed to execute in order.
    - special ops: CAS (compare-and-swap), conceptually atomic execution of:
        T atomic<T>::exchange( T desired ){ T oldval = this->value; this->value = desired; return oldval; }
        bool atomic<T>::compare_exchange_strong(T& expected, T desired){
          if( this->value == expected ){ this->value = desired; return true; }
          expected = this->value; return false;
        }

About compare_exchange: weak vs strong 
(in C++11 CAS -> compare_exchange_* and it can be read as: Am I the one who gets to change value from expected to desired? if(value.compare_exchange_strong(expected, desired)) )
  - _weak allows spurious failures
  - prefer _weak when you are going to write a CAS loop anyway
  - almost always want _strong when doing a singl test

**** Other Restrictions on Compilers and Hardware:


**** Code Gen & Performance: x86/64. IA64. POWER, ARM:


**** Relaxed Atomics:


*/
