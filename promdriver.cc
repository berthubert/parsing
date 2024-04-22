#include "promparser.hh"
#include <fmt/ranges.h>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace std;

static string readFileFrom(const char* fname)
{
  std::ifstream t(fname);
  std::stringstream buffer;
  buffer << t.rdbuf();
  return buffer.str();
}

int main(int argc, char** argv)
{
  if(argc != 2) {
    fmt::print("Run as: ./promparse prometheus.txt\n");
    return 0;
  }
  
  PromParser pp;
  auto result = pp.parse(readFileFrom(argv[1]));
  //fmt::print("Got {} names\n", result.size());

  nlohmann::json j;
  for(auto& r : result) {
    nlohmann::json inner;
    inner["help"] = r.second.help;
    inner["type"] = r.second.type;

    nlohmann::json values = nlohmann::json::array();
    for(auto& v : r.second.vals) {
      nlohmann::json value;
      value["labels"] = v.first;
      value["value"] = v.second.value;
      value["timestamp"] = v.second.tstampmsec;
      values.push_back(value);
    }
    inner["values"]=values;
    j[r.first] = inner;
  }
  fmt::print("{}\n", j.dump(1));
    
  fmt::print("{} {} {} {} - {}\n",
	     result["nvme_temperature_celsius"].vals.begin()->first.begin()->second,
	     result["nvme_temperature_celsius"].vals.begin()->second.value,
	     result["nvme_temperature_celsius"].vals.begin()->second.tstampmsec,
	     result["nvme_temperature_celsius"].help,
	     result["nvme_temperature_celsius"].type
	     );

  fmt::print("{} {} {} {} - {}\n",
	     result["go_gc_duration_seconds"].vals.begin()->first.begin()->second,
	     result["go_gc_duration_seconds"].vals.begin()->second.value,
	     result["go_gc_duration_seconds"].vals.begin()->second.tstampmsec,
	     result["go_gc_duration_seconds"].help,
	     result["go_gc_duration_seconds"].type
	     );

  fmt::print("{}\n", result["go_memstats_mspan_sys_bytes"].vals.begin()->second.value);
  fmt::print("{}\n", result["go_memstats_mcache_sys_bytes"].vals.begin()->second.value);
  fmt::print("{}\n", result["go_memstats_mspan_inuse_bytes"].vals.begin()->second.value);    
}
