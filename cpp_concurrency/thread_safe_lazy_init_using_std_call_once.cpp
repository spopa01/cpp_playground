#include <iostream>
#include <thread>
#include <mutex>
#include <vector>

class some_static_data{
private:
  std::once_flag flag;
  int count = 0;

  void init(  ){ ++count; }

public:
  
  int get_data(){
    //load static data ONCE no matter how many threads and how many times this function is called
    //so calling get_data mutiple times from multiple threads should always return 1
    std::call_once( flag, &some_static_data::init, this );
    return count;
  }

};

//Compile: g++ file_name.cpp -std=c++11 -lpthread

int main(){
  some_static_data st;

  bool err{false};

  std::vector<std::thread> pool;
  for( int i=0; i<10; ++i )
    pool.emplace_back( std::thread([&st, &err](){ for( int x=0; x<100000; ++x ){ if( st.get_data() != 1 ){ err = true; } } }) );
  for( auto& th : pool ) th.join();

  std::cout << "test..." << ( err ? "failed" : "passed" ) << "\n";
}
