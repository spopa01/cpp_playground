#include <iostream>
#include <thread>
#include <mutex>

//imagine some huge objects, swap would take long time...

struct object{
  object( int data ) : data_{data} {}

  std::mutex mutex_;
  int data_;
};

//bad
void swap1( object &d1, object &d2 ){
  std::lock_guard<std::mutex> lk1{ d1.mutex_ };
  std::lock_guard<std::mutex> lk2{ d2.mutex_ };

  std::swap( d1.data_ , d2.data_ );
}

//std::lock can lock two or more mutexes at once without the risk of deadlock...

//good
void swap2( object &d1, object &d2 ){
  if( &d1 == &d2 ) return;
  std::lock( d1.mutex_, d2.mutex_ );
  std::lock_guard<std::mutex> lk1{ d1.mutex_, std::adopt_lock };
  std::lock_guard<std::mutex> lk2{ d2.mutex_, std::adopt_lock };

  std::swap( d1.data_ , d2.data_ );
}

//also good (std::unique_lock more flexible (because it does not have to own the mutex) but a bit larger and a bit slower than std::lock_guard)
void swap3( object &d1, object &d2 ){
  if( &d1 == &d2 ) return;
  std::unique_lock<std::mutex> lk1{ d1.mutex_, std::defer_lock };
  std::unique_lock<std::mutex> lk2{ d2.mutex_, std::defer_lock };
  std::lock( lk1, lk2 );

  std::swap( d1.data_ , d2.data_ );
}

//Compile: g++ file_name.cpp -std=c++11 -lpthread

typedef void (*pf)( object &d1, object &d2 );
int main(int argc, char **argv){
  object a{1}, b{2}, c{3};
  bool run{true};

  pf swap = nullptr;
  if( argc == 2 && std::string(argv[1]) == "-swap1" ){
    std::cout << "Manually lock mutexes (using only std::lock_guard) => Deadlock\n";
    swap = &swap1;
  }if( argc == 2 && std::string(argv[1]) == "-swap2" ){
    std::cout << "std::lock + std::lock_guard => Ok\n";
    swap = &swap2;
  }if( argc == 2 && std::string(argv[1]) == "-swap3" ){
    std::cout << "std::lock + std::unique_lock => Ok\n";
    swap = &swap3;
  }

  if( swap == nullptr ){
    std::cout << "Usage: binary -swap1|swap2|swap3\n";
    return 0;
  }

  //these threads will lock and swap the data between objects...
  std::thread t1( [&run, &a, &b, swap](){ while( run ){ swap( a, b ); } } );
  std::thread t2( [&run, &b, &c, swap](){ while( run ){ swap( b, c ); } } );
  std::thread t3( [&run, &c, &a, swap](){ while( run ){ swap( c, a ); } } );

  std::this_thread::sleep_for( std::chrono::milliseconds(100) );

  run = false;

  t1.join();
  t2.join();
  t3.join();

  std::cout << a.data_ << " " << b.data_ << " " << c.data_ << "\n";

  return 0;
}
