#include "peglib.h"
#include <fmt/ranges.h>
using namespace std;

// run as ./hello "(1, 2,   0.3, -.12, +12)"

int main(int argc, char** argv)
{ 
  peg::parser p(R"(
Coord   <- '(' Number ( ',' Number )* ')'
Number <- [+-]?[0-9]*([.][0-9]*)?
%whitespace <- [\t ]*
)");

  if(!(bool)p) {
    fmt::print("Error in grammar\n");
    return 0;
  }
  
  p["Number"] = [](const peg::SemanticValues &vs) {
    return vs.token_to_number<double>();
  };

  p["Coord"] = [](const peg::SemanticValues &vs) {
    vector<double> ret;
    for(const auto& v : vs)
      ret.push_back(any_cast<double>(v));
    return ret;
  };
  
  p.set_logger([](size_t line, size_t col, const string& msg) {
    fmt::print("Error on line {}:{} -> {}\n", line, col, msg);
  });

  vector<double> result;
  auto ok = p.parse(argv[1], result);

  fmt::print("Parse result of '{}' (ok {}): {}\n",
             argv[1], ok, result);
  
}
