#include <iostream>

//Compile: g++ compile_time_magic.cpp -std=c++14

//-- Internal DSL:
//Supported operations are:
//  begin
//  end
//  open
//  close

struct state{
  void begin(){ depth = 0; }
  void open() { depth++; };
  void close() { depth--; };
  auto end(){ return depth; }

private:
  int depth = 0;
};

//At depth > 0 you can only open and close
template<int depth> struct fluent_syntax_impl{
  fluent_syntax_impl( state& s ) : s_{s} {}

  auto open(){ s_.open(); return fluent_syntax_impl<depth+1>{s_}; }
  auto close(){ s_.close(); return fluent_syntax_impl<depth-1>{s_}; }

private:
  state& s_;
};

//After begin / or at depth 0 you can only use open or end
template<> struct fluent_syntax_impl<0>{  //specialize the template for this depth
  fluent_syntax_impl( state& s ) : s_{s} {}

  auto open(){ s_.open(); return fluent_syntax_impl<1>{s_}; }
  auto end(){ return s_.end(); }

private:
  state& s_;
};

//This is the starting point
struct fluent_syntax{ 
  auto begin() { return fluent_syntax_impl<0>{s_}; }
private:
  state s_;
};

int main(){
  //only valid syntax will compile...
  auto depth =
  fluent_syntax()
    .begin()
      .open()
        .open()
          .open()
          .close()
        .close()
      .close()
    .end();

  //it can only be zero
  std::cout << "depth: " << depth << std::endl;
  
  //it does not compile...
  /*
  depth =
  fluent_syntax()
    .begin()
      .open()
    .end();
  */
  
  //it does not compile...
  /*
  depth =
  fluent_syntax()
      .open()
      .close()
    .end();
  */

  return 0;
}
