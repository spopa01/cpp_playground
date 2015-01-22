/*

http://www.drdobbs.com/parallel/writing-lock-free-code-a-corrected-queue/210604448
http://www.drdobbs.com/parallel/writing-a-generalized-concurrent-queue/211601363

*/

#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>

//---------------------------------------------------------------------------------------------------------------------------

/* 
One Producer - One Consumer Lock-Free Queue

So the producer and consumer always work in different parts of the underlying linked list.

The lock-free queue data-structure;
******|** -> ******|** -> ******|** -> ******|** -> ******|** -> ... ******|** -<>
first        divider                                                 last

The first "unconsumed" iten is the one after divider.
The consumer increments divisor to say it has consumed an item.
The producer increments last to say it has produced an item, also lazily? cleans up consumed items before the divisor.

The constructor simply initializes the list with a dummy element.
The destructor releases the list.

Ownership rules:
++++++|++ -> xxxxxx|xx -> xxxxxx|xx -> xxxxxx|xx -> ... xxxxxx|++ -<>
first        divider                                    last

++++++++  owned by the producer
xxxxxxxx  owned by the consumer
xxxxxx++  the node owned by consumer but the "next" pointer inside the node owned by producer

So:
- the producer owns all nodes before divider, the next node inside the last node and the ability to update first and last.
- the consumer owns everything else, including the values in the nodes from divisior onwards, and the ability to update divisor.
*/


template<typename T>
class lock_free_queue_t{
 private:
  struct node_t{
    node_t( T value ) 
      : value_{value}, 
        next_{nullptr} 
    {}

    T value_;
    node_t *next_;
  };

  node_t *first_;                        //producer only
  std::atomic<node_t*> divider_, last_;  //shared

public:
  lock_free_queue_t(){
    first_ = divider_ = last_ = new node_t( T() ); //dummy separator
  }

  ~lock_free_queue_t(){
    while( first_ != nullptr ){
      auto tmp = first_;
      first_ = tmp->next_;
      delete tmp;
    }  
  }
  
  void push( T const& t  ){ //called only by the producer...
    auto last = last_.load();
    last->next_ = new node_t(t); //add a new node
    last_ = last->next_;         //publish it

    while( first_ != divider_ ){  //trim unused nodes
      auto tmp = first_;
      first_ = first_->next_;
      delete tmp;
    }
  }

  bool pop( T& t ){         //called only by the consumer...
    if( divider_ != last_ ){      //if queue is not empty
      auto next = (divider_.load())->next_;
      t = next->value_;           //copy the value
      divider_ = next;            //publsh that we took it (by advancing divider)
      return true;                //report success
    }
    return false;                 //report empty
  }
};

//---------------------------------------------------------------------------------------------------------------------------

/*
Multiple Producers - Multiple Consumers 

Design:
- the code is using two spinlocks (one for producers and one for consumers)
- data stored in the nodes is allocated on the heap and the nodes hold only the pointer to it ( it seems that yields better performance )
- now, the consumers will trim the consumed nodes
- keep everything on different cache lines ( it did not make too much difference on my macbook or ubuntu14_10 vm )

Also:
- the underlying data-structure rermains linked list
- no more divider
- now the next pointers become shared variables and need to be protected (in this case they are defined as ordred atomic types)

Structure of an empty queue:

  #
  |   
+++++|+++++ -#
first/last

A queue containing objects:

  #              T                  T
  |              |                  |
+++++|+++++ -> +++++|+++++ -> ... +++++|+++++ -#
first                             last

*/

#define CACHE_LINE_SIZE 16

struct alignas(CACHE_LINE_SIZE) spin_lock_t{
  std::atomic_flag flag;

  spin_lock_t() : flag( ATOMIC_FLAG_INIT ) {}

  //spin + yield seems to improve the performance...
  void lock(){ while( flag.test_and_set( std::memory_order_acquire ) ){ std::this_thread::yield(); }; }
  //void lock(){ while( flag.test_and_set( std::memory_order_acquire ) ); }

  void unlock(){ flag.clear( std::memory_order_release ); }
};

template<typename T>
class concurrent_queue_t{
private:
  struct alignas(CACHE_LINE_SIZE) node_t{
    node_t( T* value )
      : value_{value},
        next_{nullptr}
    {}

    T* value_;
    std::atomic<node_t*> next_;
    char pad[CACHE_LINE_SIZE - sizeof(T*) - sizeof(std::atomic<node_t*>)];
  };
  
  //because we force the alignment we only need padding at the end...
  //char pad0[CACHE_LINE_SIZE];
  alignas(CACHE_LINE_SIZE) node_t *first_;
  //char pad1[CACHE_LINE_SIZE - sizeof(node_t*)];
  alignas(CACHE_LINE_SIZE) node_t *last_;
  //char pad2[CACHE_LINE_SIZE - sizeof(node_t*)];
  spin_lock_t producer_lock_;
  //char pad3[CACHE_LINE_SIZE - sizeof(spin_lock_t)];
  spin_lock_t consumer_lock_;
  char pad[CACHE_LINE_SIZE - sizeof(spin_lock_t)];

public:
  concurrent_queue_t(){
    first_ = last_ = new node_t( nullptr );
  }

  ~concurrent_queue_t(){
    while( first_ != nullptr ){
      auto tmp = first_;
      first_ = tmp->next_;
      delete tmp->value_; //no-op if null
      delete tmp;
    }
  }

  void push( T const& t ){
    auto tmp = new node_t( new T(t) );
    { std::lock_guard< spin_lock_t > lk{ producer_lock_ };
      last_->next_ = tmp;     //publish to consumer
      last_ = tmp;            //swing last_ forward
    }
  }

  bool pop( T& t ){
    std::unique_lock< spin_lock_t > lk{ consumer_lock_ };

    if( first_->next_ != nullptr ){   //if the queue is not empty
      auto old_first = first_;
      first_ = first_->next_;

      auto val = first_->value_;      //take the value out of the node
      first_->value_ = nullptr;
      lk.unlock();                    //release the lock

      t = *val;

      delete val;                     //cleanup
      delete old_first;
      return true;
    }

    return false;
  }
};

//---------------------------------------------------------------------------------------------------------------------------

#define SAMPLES 1000000
#define SPLITS 4

void test_lock_free_queue(){
  lock_free_queue_t<int> qu;
  
  bool err {false};

  auto start = std::chrono::high_resolution_clock::now();
  std::thread tw( [&qu](){ for( int i=0; i<SAMPLES; ++i ){ qu.push( i ); } } );
  std::thread tr( [&qu, &err](){ int i=0; int t; while(i<SAMPLES){ if(qu.pop(t)){ if( i!=t ){ err=true; } ++i; } }; } );
  tw.join();
  tr.join();
  
  auto stop = std::chrono::high_resolution_clock::now();
  auto elapsed = stop - start;

  std::cout << "test..." << ( err ? "failed" : "passed" ) << "\n";
  std::cout << "elapsed: " << std::chrono::duration_cast<std::chrono::milliseconds>( elapsed ).count() << "\n";
};

void test_concurrent_queue_1(){
  concurrent_queue_t<int> qu;
  
  bool err {false};

  auto start = std::chrono::high_resolution_clock::now();
  std::thread tw( [&qu](){ for( int i=0; i<SAMPLES; ++i ){ qu.push( i ); } } );
  std::thread tr( [&qu, &err](){ int i=0; int t; while(i<SAMPLES){ if(qu.pop(t)){ if( i!=t ){ err=true; } ++i; } }; } );
  tw.join();
  tr.join();

  auto stop = std::chrono::high_resolution_clock::now();
  auto elapsed = stop - start;

  std::cout << "test..." << ( err ? "failed" : "passed" ) << "\n";
  std::cout << "elapsed: " << std::chrono::duration_cast<std::chrono::milliseconds>( elapsed ).count() << "\n";
}

#include <algorithm>
void test_concurrent_queue_2(){
  concurrent_queue_t<int> qu;

  std::vector<int> v1( SAMPLES ), v2( SAMPLES, 0 );
  std::generate( v1.begin(), v1.end(), [](){ static int i=0; return i++; } );

  auto start = std::chrono::high_resolution_clock::now();
  std::atomic<int> idx{0};
  std::vector<std::thread> pool;
  for( int id=0; id<SPLITS; ++id ){
    pool.emplace_back( std::thread( [id, &v1, &qu](){ for( int i=0; i<SAMPLES/SPLITS; ++i ){ qu.push( v1[ id*SAMPLES/SPLITS + i ] ); } } ) );
    pool.emplace_back( std::thread( [id, &idx, &v2, &qu](){ int v; while(idx < SAMPLES){ if(qu.pop(v)){ v2[idx++] = v; } } } ) );
  }

  for( auto& th : pool ) th.join();
  
  auto stop = std::chrono::high_resolution_clock::now();
  auto elapsed = stop - start;

  std::sort( v2.begin(), v2.end() );

  std::cout << "test..." << ( v1 != v2 ? "failed" : "passed" ) << "\n";
  std::cout << "elapsed: " << std::chrono::duration_cast<std::chrono::milliseconds>( elapsed ).count() << "\n";
}

//---------------------------------------------------------------------------------------------------------------------------

//Compile: g++ file_name.cpp -std=c++11 -lpthread -O4

/*

Example (time in milliseconds):

test...passed
elapsed: 122
test...passed
elapsed: 404
test...passed
elapsed: 302

*/

int main( /**/ ){
  test_lock_free_queue();
  test_concurrent_queue_1();
  test_concurrent_queue_2();
  return 0;
}
