#include <iostream>

#include <thread>
#include <future>
#include <functional>

/*

->A

The C++ Standard Library models one-off event scenario with something called "futures".

The mechanism of using futures can be summarized as follow:
- if a thread needs to wait for a specific one-off event, it has to somehow (SEE PART B) obtain a future representing this event.
Further:
- the thread can then periodically wait on the future for short periods of time to see if the event has occured while performing some other task in between checks.
Or
- alternatively it can do another task until it needs the event to have happened before it can proceed any further, so it has to wait for the future to become "ready".

C++ STL also provides shared_futures (as an alternative to the basic (unique) futures). The two are moedelled after the std::unique_ptr and std::shared_ptr (which means that 
in the case of the shared_future there may be multiple instances pointing to the same event and they all become "ready" in the same time and all may acccess the data)
so futures are only movable, and shared_futures are copyable.
There is also a special type of future (std::future<void> and std::shared_future<void>) which can be used only to signal the end of a task/stateless events.

Another important note here is that, although futures are used to communicate between threads, the future objects themselves don't provide synchronized access.
If multiple threads need to access a single future, they must protect the access via a mutex or other synchronization mechanism, otherwise they have a data race and undefined behaviour
(because after the get() function is called from one of the threads the future becomes invalid)

A solution for this problem is to have a copy of a shared_future for each thread who needs to wait for that one-off event (this weoks for shared_future because they are copyable, 
compared with future which is only movable).

Another interesting fact is that futures can store a result or an exception.

-> B
A future can be obtained in 3 different ways:
- using std::async (easiest way)
- using std::packeged_task
- using std::promise (lowest level abstraction)

Using std::async.
std::async starts an asynchronous task for which you don't need result right away. So rathrer than giving you a std::thread object to wait on, async returns a std::future object,
which will eventually hold the return value of the function.
Note: std::async accepts also a parameter of type std::launch, which can be:
      - deferred to indicate that the function call representing the task is made when the wait/get is called on the future
      - async to indicate that the function call representing the task must be made on it's own thread

Using std::packeged_task
std::packaged_task is used to tie a future to a function or callable object; when the pakaged_task object is invoked, it calls the associated function or callable object and
makes the future ready with the return value stored as the associated data. 
std::packaged_task is not as easy to use as async but provides more flexibility, and it can be used as building block for thread pools and other more advanced task management schemas.
Once you have created an packaged_task you can call get_future on it in order to get a future.

Using std::promise
std::promise<T> simply provides the means of directly setting a value (of given type T) which can later be read through an associated std::future<T>.
A future can be simply obtained from the promise by calling get_future. The value (on the promise side) can be simply be set by calling set_value.

*/

void test_async_usage(){
  //by default it is called using std::launch::deferred | std::launch::async - which allows the implementation to take the decision.
  std::future<int> answer = std::async( [](){ std::this_thread::sleep_for(std::chrono::milliseconds(100)); return 100; } ); //do something
  
  std::this_thread::sleep_for(std::chrono::milliseconds(50)); //do somthing else in the same time with the async task

  //get back the answer
  auto x = 100 + answer.get();

  std::cout << "test_async_usage..." << (x == 200 ? "passed" : "failed") << "\n";
}

void test_packaged_task_usage(){
  //create a function to run as packaged_task (in this case a lambda)
  auto func = [](int x){ return 2*x; }; //int(void) signature

  //create the packaged_task using func
  std::packaged_task< int(int) > task1(func);
  std::packaged_task< int(void) > task2(std::bind(func, 100));

  //now obtain a future from the task (future has template parameter the same type as the return type of the function packaged as a task)
  std::future<int> fut1 = task1.get_future();
  std::future<int> fut2 = task2.get_future();

  // now run the packaged_task on a thread pool / in this case it is going to be a simple thread...
  std::thread t1( [&task1](){ task1(100); } ); 
  std::thread t2( std::move(task2) );
  
  t1.join();
  t2.join();

  //now get the results
  auto x = fut1.get() + fut2.get();

  std::cout << "test_packaged_task..." << (x == 400 ? "passed":"failed") << "\n";
}

void test_promise_usage(){
  std::promise<int> p;
  std::future<int> f(p.get_future());
  
  std::thread t( [&p](){ p.set_value(100); } );
  auto x = f.get();
  t.join();

  std::cout << "test_promise_usage..." << (x == 100 ? "passed":"failed") << "\n";
}

void test_movable_copyable(){
  bool pass{true};

  std::promise<void> p1;
  std::future<void> f1(p1.get_future()); //get somehow a future.
  pass = pass && f1.valid(); // the future has to be valid at this point.
  
  std::shared_future<void> sf1(std::move(f1)); // because the future does not share ownership, in this case it must be explicitly thransfered into the shared_future.
  pass = pass && !f1.valid(); //the future has to be invalid at this point
  pass = pass && sf1.valid(); //but the shared_future has to be valid

  std::promise<void> p2;
  std::shared_future<void> sf2(p2.get_future()); //implicit transfer of ownership, the future returned by get_future is an rvalue.
  pass = pass && sf2.valid();

  std::shared_future<void> sf3(sf2);
  pass = pass && sf3.valid();
  pass = pass && sf2.valid(); //now both, 2 and 3 must be valid.

  std::cout << "test_future_movable_shared_future_copyable..." << (pass ? "passed" : "failed") << "\n";
}

void test_multiple_copies_of_shared_future(){
  std::promise<void> p;
  std::shared_future<void> sf(p.get_future());

  std::thread t1( [sf](){ sf.get(); } ); //t1 owns a copy
  std::thread t2( [sf](){ sf.get(); } ); //t2 owns another copy
  // but all threads can receive the result of that promise... and no explicit synchronization needed

  p.set_value();

  t1.join();
  t2.join();

  std::cout << "test_multiple_copies_of_shared_future...ok\n";
}

//Compile: g++ waiting_for_one_off_event_with_futures.cpp -std=c++11 -lpthread

int main(/*...*/){
  test_async_usage();
  test_packaged_task_usage();
  test_promise_usage();

  test_movable_copyable();
  test_multiple_copies_of_shared_future();

  return 0;
}
