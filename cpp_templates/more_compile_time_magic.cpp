#include <iostream>
#include <string>
#include <sstream>

//Compile: g++ more_compile_time_magic.cpp -std=c++14

using key = std::string;
using val = std::string;

#define T first = true; 
#define F first = false; 
#define IST (first ? " " : ", ")

struct state{
  void begin() { ss << "{ "; T }
  std::string end() { ss << " }\n"; return ss.str(); }

  void begin_object( key const& k ){ ss << IST << k << ": { "; T }
  void begin_object(){ ss << IST << " { "; T }
  void end_object(){ ss << " } "; F }
  
  void begin_array( key const& k ){ ss << IST << k << ": [ "; T }
  void begin_array(){ ss << IST << " [ "; T }
  void end_array(){ ss << " ] "; F }

  void add_entry( key const& k, val const& v ){ ss << IST << k << ": " << v << " "; F }
  void add_entry( val const& v ){ ss << IST << v << " "; F }

private:
  bool              first;
  std::stringstream ss;
};

//-- Internal DSL:
//Supported operations are:
//  begin
//  end
//  begin_object
//  end_object
//  begin_array
//  end_array
//  add_entry

#define xobj 0
#define xarr 1
#define xroot 2

template<unsigned int closure, unsigned int depth, unsigned int hist> struct fluent_syntax_impl;

//At depth > 0 and inside an object you can use begin obj/arr and end obj
template<unsigned int depth, unsigned int hist> struct fluent_syntax_impl<xobj, depth, hist>{  //specialize the template for this closure
  fluent_syntax_impl( state& s ) : s_{s} {}
  
  auto begin_object( key const& k ){ s_.begin_object(k); return fluent_syntax_impl< xobj, depth+1, ((hist<<1)|xobj)>{s_};  }
  auto begin_array( key const& k ){ s_.begin_array(k); return fluent_syntax_impl< xarr, depth+1, ((hist<<1)|xobj)>{s_};  }
  
  auto add_entry( key const& k, val const& v ){ s_.add_entry(k,v); return fluent_syntax_impl< xobj, depth, hist>{s_};  }

  auto end_object(){ s_.end_object(); return fluent_syntax_impl< ((depth>1) ? ( (hist&xarr) ? xarr : xobj ) : xroot ), depth-1, (hist>>1)>{s_};  }

private:
  state& s_;
};

//At depth > 0 and inside an array you can use begin obj/arr and end arr
template<unsigned int depth, unsigned int hist> struct fluent_syntax_impl<xarr, depth, hist>{  //specialize the template for this closure
  fluent_syntax_impl( state& s ) : s_{s} {}
  
  auto begin_object(){ s_.begin_object(); return fluent_syntax_impl< xobj, depth+1, ((hist<<1)|xarr) >{s_};  }
  auto begin_array(){ s_.begin_array(); return fluent_syntax_impl< xarr, depth+1, ((hist<<1)|xarr) >{s_};  }

  auto add_entry( val const& v ){ s_.add_entry(v); return fluent_syntax_impl< xarr, depth, hist>{s_};  }

  auto end_array(){ s_.end_array(); return fluent_syntax_impl< ((depth>1) ? ( (hist&xarr) ? xarr : xobj ) : xroot ), depth-1, (hist>>1)>{s_};  }

private:
  state& s_;
};

//After begin / or at depth == 0 you can use begin obj/arr or end
template<> struct fluent_syntax_impl<xroot, 0, 0>{  //specialize the template for this depth
  fluent_syntax_impl( state& s ) : s_{s} {}
  
  auto begin_object( key const& k ){ s_.begin_object(k); return fluent_syntax_impl< xobj, 1, 0 >{s_};  }
  auto begin_array( key const& k ){ s_.begin_array(k); return fluent_syntax_impl< xarr, 1, 0 >{s_};  }

  auto end(){ return s_.end(); }

private:
  state& s_;
};

//This is the starting point
struct fluent_syntax{ 
  auto begin() { s_.begin(); return fluent_syntax_impl<xroot, 0, 0>{s_}; }
private:
  state s_;
};

int main(){

  //only valid syntax will compile...
  std::cout <<
  fluent_syntax()
    .begin()
      .begin_object( "earthling" )
        .add_entry( "first_name", "stefan" )
        .add_entry( "sure_name", "popa" )
        .begin_array( "says" )
          .add_entry( "hello" )
          .add_entry( "world..." )
          .begin_array()
            .add_entry( "pam..." )
            .add_entry( "pam..." )
          .end_array()
        .end_array()
        .begin_object("description")
          .add_entry("mind", "funny")
          .add_entry("body", "unknown")
        .end_object()
      .end_object()
    .end();
/*
Generates well formatted... JSON like output
{  earthling: {  first_name: stefan , sure_name: popa , says: [  hello , world... ,  [  pam... , pam...  ]  ] , description: {  mind: funny , body: unknown  }  }  }
*/
  return 0;
}
