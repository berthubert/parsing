#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <map>

namespace peg {
  struct parser;
}

class PromParser
{
public:
  PromParser();
  ~PromParser();
  struct TstampedValue
  {
    int64_t tstampmsec;
    double value;
  };
  
  struct PromEntry
  {
    std::string help;
    std::string type;
    std::map<std::map<std::string,std::string>, TstampedValue> vals;
  };

  
  typedef std::map<std::string, PromEntry> promparseres_t;
  promparseres_t parse(const std::string& in);
  
private:
  std::unique_ptr<peg::parser> d_p;
  std::string d_error;
};
