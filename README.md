# cpp_playground
Just a playground... exercising my inner child

## cpp_concurrency (in action):
```C++
//g++ have_fun.cpp -std=c++11 -lpthread

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

int main( /*...*/ ){
  std::atomic<bool> pause {true};
  //children are playing...
  std::thread child([&pause](){ while( pause ){ std::cout << "Have fun..." << std::endl; } });
  //so the parents can sleep...
  std::this_thread::sleep_for( std::chrono::milliseconds(1) );
  pause = false;
  child.join();
  std::cout << "Go back to lessons..." << std::endl;
  return 0;
}
```
