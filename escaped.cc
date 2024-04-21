#include "peglib.h"
#include <fmt/ranges.h>
using namespace std;

int main(int argc, char** argv)
{
  if(argc != 2) {
    fmt::print("Run as: ./escaped '\"hello this is a \\\"string\\\"\"'\n");
    return 0;
  }
  peg::parser p(R"(
QuotedString   <- '"' String '"'
String <- (! '"' Char )*
Char <- ('\\' < . > ) / (!'\\' .)
)");

  if(!(bool)p) {
    fmt::print("Error in grammar\n");
    return 0;
  }
  
  p["String"] = [](const peg::SemanticValues &vs) {
    string ret;
    for(const auto& v : vs) {
      ret += any_cast<string>(v);
    }
    return ret;
  };

  p["QuotedString"] = [](const peg::SemanticValues &vs) {
    return any_cast<string>(vs[0]);
  };

  p["Char"] = [](const peg::SemanticValues &vs) {
    fmt::print("Char returning: {}\n", vs.token_to_string());
    string res = vs.token_to_string();
    if(vs.choice() == 0) { // this was an escape
      if(res=="n") {
        res = "\n";
      }
    }
    return res;
  };

  
  p.set_logger([](size_t line, size_t col, const string& msg) {
    fmt::print("Error on line {}:{} -> {}\n", line, col, msg);
  });

  string result;
  auto ok = p.parse(argv[1], result);

  fmt::print("Parse result of '{}' (ok {}): {}\n",
             argv[1], ok, result);
  
}
