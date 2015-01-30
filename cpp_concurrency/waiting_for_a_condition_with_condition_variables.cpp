#include <thread>
#include <mutex>
#include <condition_variable>

#include <queue>
#include <chrono>
#include <iostream>

/*
Here, we discuss the situation where a thread has to wait for a second thread to finish to complete a task.

To accomplish this the first thread has several options:

- a first option could be checking a flag in shared data (protected by a mutex) and have the second thread set this flag when it complete the task
  ( this is bad/wasteful from 2 main reasons: the thread consumes valuable processing time when repeatedly checking the flag 
    and when the mutex protecting the flag is locked by the waiting thread, it can't be locked by any other thread )

- a second option (usually better) could be to have the first thread sleep for small periods between the flag checks. Something like:
  bool flag;
  st::mutex m;

  void w8_for_flag(){
    std::unique_lock<std::mutex> lk{m};
    while( !flag ){
      lk.unlock();
      std::this_thread::sleep_for( std::chrono::milliseconds(100) );
      lk.lock();
    }
  }
  The bad thing about this approach is that usually it is hard to right the right value for the sleep. 
  Too small you are in the first case, too long and you may end up with a thread doing almost nothing other than sleep.

- a third (prefereed) option is to use the C++ Standerd Library facilities. This facility is called a "condition variable".
  A condition variable is associated with some event or condition and one or more threads can "wait" for the condition to be satisfied.
  When some thread thread has determined thant the condition is satisfied, it can then "notify" one/all of the threads waiting on the condition variable
  in order to wake them up and allow them to continue processing.

*/

void basic_producer_consumer_1(){
  bool e{false};

  std::mutex m;//mutex to protect the data
  std::condition_variable c;//condition variable used for signaling the presence of data
  
  std::queue<unsigned int> q;//data

  std::thread producer([ &m, &c, &q ](){
      unsigned int i=1;
      while( i <= 5 ){
        {
        std::lock_guard<std::mutex> lk{m};    //lock
        q.push( i );                          //push some data
        c.notify_one();                       //notify
        std::cout << "produced:" << i << "\n";
        i++;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds(100) ); //this is just for the demo...
      }
    });
  
  std::thread consumer([ &m, &c, &q, &e ](){
      unsigned int i=1;
      while( i <= 5 ){
        std::unique_lock<std::mutex> lk{m};       //create a lock
        //set the condition; also note that the condition needs a mutex, but during the sleep, the mutex is not locked/owned by this thread
        c.wait(lk, [&q](){ return !q.empty(); });
        unsigned int x = q.front(); q.pop();      //now the thread has been notified, is awake, the mutex is locked and data consumed
        if( i != x ) e = true;
        std::cout << "consumed:" << x << "\n";
        i++;
      }
    });

  producer.join();
  consumer.join();

  std::cout << "basic_producer_consumer_test_1..." << (e ? "failed" : "passed") << "\n";
}

template<typename T>
struct concurrent_queue{

  void push( T const& v ){
    std::lock_guard<std::mutex> lk{m};
    q.push(v);
    c.notify_one();
  }

  void pop( T& v ){
    std::unique_lock<std::mutex> lk{m};
    c.wait(lk, [this](){ return !q.empty(); });
    v = q.front(); q.pop();
  }

private:
  std::mutex m;
  std::condition_variable c;
  std::queue<T> q;
};

void basic_producer_consumer_2(){
  bool e{false};
  concurrent_queue<unsigned int> q;

  std::thread producer([ &q ](){
      unsigned int i=1;
      while( i <= 5 ){
        std::cout << "produced:" << i << "\n";
        q.push( i );                          //push some data
        i++;
        std::this_thread::sleep_for( std::chrono::milliseconds(100) ); //this is just for the demo...
      }
    });
  
  std::thread consumer([ &q, &e ](){
      unsigned int i=1;
      while( i <= 5 ){
        unsigned int x; q.pop(x);
        if( i != x ) e = true;
        std::cout << "consumed:" << x << "\n";
        i++;
      }
    });

  producer.join();
  consumer.join();

  std::cout << "basic_producer_consumer_test_2..." << (e ? "failed" : "passed") << "\n";
}


//Compile: g++ file.cpp -std=c++11 -lpthread

int main(/*...*/){
  basic_producer_consumer_1();
  basic_producer_consumer_2();
  return 0;
}
