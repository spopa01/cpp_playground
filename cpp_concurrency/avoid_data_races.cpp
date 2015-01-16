#include <iostream>
#include <chrono>
#include <set>
#include <thread>
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <boost/thread/shared_mutex.hpp>


/*
A data race occurs when (all the following happen):
- two / more threads (in a single process) access the same memory location concurrently 
- at least one of the accesses is for writing
(- the threads are not using any exclusive locks to control their access to the memory...)

When all 3 happen the order of accesses is nondeterministic and leads to the corruption of the data...

Some data-races may be intentional (benign) like busy-wait constructs...

The cure ... is protect your state using synchronization primitives...

*/

#define SAMPLE_SIZE 1000000
#define GRANULARITY 5

//safe read/write
struct cache1{
  void add( int val ){
    std::lock_guard<std::mutex> lk{m_};
    cache_.insert( val );
  }

  bool contains( int val ){
    std::lock_guard<std::mutex> lk{m_};
    return cache_.count(val);
  }

  void lock(){ m_.lock(); }
  void unlock(){ m_.unlock(); }

  std::mutex m_;
  std::set<int> cache_;
};

//some sort of data structure optimized for reading but still safe for writing...

//using std::shared_timed_mutex
struct cache2{
  void add( int val ){
    //exclusive ownership
    std::lock_guard<std::shared_timed_mutex> lk{m_};
    cache_.insert( val );
  }

  bool contains( int val ){
    //shared ownership
    std::shared_lock<std::shared_timed_mutex> lk{m_};
    return cache_.count(val);
  }
  
  void lock(){ m_.lock_shared(); }
  void unlock(){ m_.unlock_shared(); }

  std::shared_timed_mutex m_;
  std::set<int> cache_;
};

//using boost::shared_mutex
struct cache3{
  void add( int val ){
    //exclusive ownership
    boost::unique_lock<boost::shared_mutex> lk{m_};
    cache_.insert( val );
  }

  bool contains( int val ){
    //shared ownership
    boost::shared_lock<boost::shared_mutex> lk{m_};
    return cache_.count(val);
  }

  void lock(){ m_.lock_shared(); }
  void unlock(){ m_.unlock_shared(); }

  boost::shared_mutex m_;
  std::set<int> cache_;
};

//spin lock - just for fun...
struct spin_lock{
  std::atomic_flag flag;

  spin_lock() : flag( ATOMIC_FLAG_INIT ) {}

  void lock(){
    while( flag.test_and_set( std::memory_order_acquire ) );
  }

  void unlock(){
    flag.clear( std::memory_order_release );
  }
};

struct cache0{
  void add( int val ){
    std::lock_guard<spin_lock> lk{m_};
    cache_.insert( val );
  }

  bool contains( int val ){
    std::lock_guard<spin_lock> lk{m_};
    return cache_.count(val);
  }

  void lock(){ m_.lock(); }
  void unlock(){ m_.unlock(); }

  spin_lock m_;
  std::set<int> cache_;
};

// fine lock granularity
template<typename C> void test1(){
  C c;

  //first let's add somthing to the cache...
  std::thread tw1([&c](){ int i=0; int m = SAMPLE_SIZE; while(i++<m){ c.add( i ); } });
  std::thread tw2([&c](){ int i=0; int m = SAMPLE_SIZE; while(i++<m){ c.add( m+i ); } });

  tw1.join();
  tw2.join();

  auto start = std::chrono::high_resolution_clock::now();

  //further let's read and write (from time to time) to the cache
  std::thread tw([&c](){ 
      int i=0; int m = SAMPLE_SIZE;
      while(i++<10){ 
        c.add( 2*m+i );
        std::this_thread::sleep_for( std::chrono::milliseconds(10) );
      }
    });
  bool cache_error {false};
  std::thread tr1([&c, &cache_error](){ 
      int i=0; int m = SAMPLE_SIZE;
      while(i++<m){ 
        if( !c.contains( i ) )
          cache_error = true;
      }
    });
  std::thread tr2([&c, &cache_error](){ 
      int i=0; int m = SAMPLE_SIZE;
      while(i++<m){ 
        if( !c.contains( m+i ) )
          cache_error = true;
      }
    });

  tw.join();
  tr1.join();
  tr2.join();
  
  auto stop = std::chrono::high_resolution_clock::now();
  auto elapsed = stop - start;
  
  std::cout << "test..." << ( cache_error ? "failed" : "passed" ) << "\n";
  std::cout << "elapsed: " << std::chrono::duration_cast<std::chrono::milliseconds>( elapsed ).count() << "\n";
}

// coarse lock granularity
template<typename C> void test2(){
  C c;

  //first let's add somthing to the cache...
  std::thread tw1([&c](){ int i=0; int m = SAMPLE_SIZE; while(i++<m){ c.add( i ); } });
  std::thread tw2([&c](){ int i=0; int m = SAMPLE_SIZE; while(i++<m){ c.add( m+i ); } });

  tw1.join();
  tw2.join();

  auto start = std::chrono::high_resolution_clock::now();

  //further let's read and write (from time to time) to the cache
  std::thread tw([&c](){ 
      int i=0; int m = SAMPLE_SIZE;
      while(i++<10){ 
        c.add( 2*m+i );
        std::this_thread::sleep_for( std::chrono::milliseconds(10) );
      }
    });
  bool cache_error {false};
  std::thread tr1([&c, &cache_error](){ 
      int i=0; int m = SAMPLE_SIZE; int n = m/GRANULARITY;
      while(i<m){
        c.lock();
        int j=0;
        while( j++ < n ){
          if( !c.cache_.count( i+j ) )
            cache_error = true;
        }
        c.unlock();
        i+=n;
      }
    });
  std::thread tr2([&c, &cache_error](){ 
      int i=0; int m = SAMPLE_SIZE; int n = m/GRANULARITY;
      while(i<m){
        c.lock();
        int j=0;
        while( j++ < n ){
          if( !c.cache_.count( m+i+j ) )
            cache_error = true;
        }
        c.unlock();
        i+=n;
      }
    });

  tw.join();
  tr1.join();
  tr2.join();
  
  auto stop = std::chrono::high_resolution_clock::now();
  auto elapsed = stop - start;
  
  std::cout << "test..." << ( cache_error ? "failed" : "passed" ) << "\n";
  std::cout << "elapsed: " << std::chrono::duration_cast<std::chrono::milliseconds>( elapsed ).count() << "\n";
}

/* Example of results (on a vm - in milliseconds)
****************************** Fine lock granularity
std::mutex
test...passed
elapsed: 635
std::shared_mutex
test...passed
elapsed: 466
boost::shared_mutex
test...passed
elapsed: 572
spin_lock
test...passed
elapsed: 527
****************************** Coarse lock grnularity
std::mutex
test...passed
elapsed: 427
std::shared_mutex
test...passed
elapsed: 221
boost::shared_mutex
test...passed
elapsed: 371
spin_lock
test...passed
elapsed: 467
******************************
*/

//Compile: g++ file_name.cpp -std=c++14 -lpthread -lboost_system -lboost_thread -O4

int main(/*...*/){
  std::cout << "****************************** Fine lock granularity\n";
  std::cout << "std::mutex\n";
  test1<cache1>();
  std::cout << "std::shared_mutex\n";
  test1<cache2>();
  std::cout << "boost::shared_mutex\n";
  test1<cache3>();
  std::cout << "spin_lock\n";
  test1<cache0>();

  std::cout << "****************************** Coarse lock grnularity\n";
  std::cout << "std::mutex\n";
  test2<cache1>();
  std::cout << "std::shared_mutex\n";
  test2<cache2>();
  std::cout << "boost::shared_mutex\n";
  test2<cache3>();
  std::cout << "spin_lock\n";
  test2<cache0>();
  std::cout << "******************************\n";
}

