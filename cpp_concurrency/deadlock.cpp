#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>

//deadlock simply describes a situation in which two or more threads are blocked (hang) forever waiting for each other.

void test_deadlock_1(){
  std::mutex m1, m2;

  std::cout << "These threads are blocked forever (by mistake someone locks the mutexes in the wrong order) ...\n";

  bool run{true};

  std::thread t1( [&run, &m1, &m2](){ while(run){ std::lock_guard<std::mutex> lk1{m1}; std::lock_guard<std::mutex> lk2{m2}; } } );
  std::thread t2( [&run, &m1, &m2](){ while(run){ std::lock_guard<std::mutex> lk1{m2}; std::lock_guard<std::mutex> lk2{m1}; } } );

  std::this_thread::sleep_for( std::chrono::milliseconds(100) );
  run = false;

  t1.join();
  t2.join();
}

void test_solve_deadlock_1(){
  std::mutex m1, m2;

  std::cout << "Solve deadlock by locking the mtexes always the in the same order...\n";
  std::cout << "Stay tuned for avoid_deadlock_with_std_lock to see even a better solution...\n";

  bool run{true};

  std::thread t1( [&run, &m1, &m2](){ while(run){ std::lock_guard<std::mutex> lk1{m1}; std::lock_guard<std::mutex> lk2{m2}; } } );
  std::thread t2( [&run, &m1, &m2](){ while(run){ std::lock_guard<std::mutex> lk1{m1}; std::lock_guard<std::mutex> lk2{m2}; } } );

  std::this_thread::sleep_for( std::chrono::milliseconds(100) );
  run = false;

  t1.join();
  t2.join();
}

//another popular way to deadlock is by locking (by mistake) a mutex twice / multiple times from the same thread...

void test_deadlock_2(){
  std::mutex m;

  std::cout << "This thread is blocked forever (by mistake someone locks the same xmutex twice on the same thread) ...\n";

  std::thread t( [&m](){ std::lock_guard<std::mutex> lk1{m}; std::lock_guard<std::mutex> lk2{m}; } );
  t.join();
}

void test_solve_deadlock_2(){
  std::recursive_mutex m;

  std::cout << "This thread does not block when it locks a mutex twice (because this is a std::recursive_mutex) ...\n";

  std::thread t( [&m](){ std::lock_guard<std::recursive_mutex> lk1{m}; std::lock_guard<std::recursive_mutex> lk2{m}; } );
  t.join();
}

//Compile: g++ file_name.cpp -std=c++11 -lpthread

int main(int argc, char **argv){
  if( argc == 2 && std::string(argv[1]) == "-test_deadlock_1" ){
    test_deadlock_1();
  }else if( argc == 2 && std::string(argv[1]) == "-test_solve_deadlock_1" ){
    test_solve_deadlock_1();
  }else if( argc == 2 && std::string(argv[1]) == "-test_deadlock_2" ){
    test_deadlock_2();
  }else if( argc == 2 && std::string(argv[1]) == "-test_solve_deadlock_2" ){
    test_solve_deadlock_2();
  }else{
    std::cout << "Usage: binary -test_deadlock_1|test_slove_deadlock_1|test_deadlock_2|test_slove_deadlock_2\n";
  }

  return 0;
}
