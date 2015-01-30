/*

One of the most important features of C++11 Standard is the new (conurrency aware) memory model.

There are two impoertant aspects to the memory model: the basic structural aspects (which relate to how things are laid out in memory) and then the concurrency aspect.

From the basic structural aspects, there are four important thinks to take away:
- every variable is an object ( now don't tkink about java/C# objects =)) ), including those that are members of other objects.
- every object occupies at least one memory location (to is important to point out for zero bitfields...)
- variables of fundamental type such as int or char are exactly one memory location, whatever their size, even if they're adjancent or part of an array.
- adjancent bit fields are part of the same memory mocation (see also observartion 2 for zero bitfields).

The crucial aspects for concurrency:
- everithing is based on these memory locations
- if two threads access separate memory locations there is no problem
- if two threads access same memory location but just for reading, again there is no problem (read-only does not require protection or synchronization)
- if either thread is modifying the data, there is a potential race condition.

In order to avoid race conditions, there has to be an enforced ordering between accesses in the two threads.
There are two ways two main ways of doing this:
- one way to impose ordering is to use mutexes (previously described)
- the other way is to use the synchronization properties of atomic operations in order to enforce.

NOTE: In the second case, although the undefined behivour is removed, the race itself is not removed, the order in which the two atomic operations touch the meory remains undefined.
      ( but after each atomic memory operation the memory location remains consistent, so the next atomic memory operation will see the memoru location in the final/consistent state... )

The atomic type are all accessed through a specialization of the std::atomic<> for the basic types (eg. int, char, unsigned int etc plus specializations for the pointer type)
There is also a special std::atomic_flag (which is the only one guaranteed/required bt the C++ Standard to be lock_free). An atomic flag is initialized to clear, and then it can be either 
queried and set (test_and_set) or cleared (clear).

Much more to come...

*/
