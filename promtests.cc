#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "promparser.hh"

using namespace std;

TEST_CASE("basic test") {
  PromParser p;
  auto res = p.parse(R"(# HELP apt_autoremove_pending Apt packages pending autoremoval.
# TYPE apt_autoremove_pending gauge
apt_autoremove_pending 149
)");
  
  REQUIRE(res.size() == 1);
  CHECK(res.begin()->first=="apt_autoremove_pending");
  map<string,string> emp;
  CHECK(res["apt_autoremove_pending"].vals[emp].value == 149);
  CHECK(res["apt_autoremove_pending"].vals[emp].tstampmsec == 0);
  CHECK(res["apt_autoremove_pending"].type == "gauge");
  CHECK(res["apt_autoremove_pending"].help == "Apt packages pending autoremoval.");
}

TEST_CASE("test with label") {
  PromParser p;
  auto res = p.parse(R"(# HELP apt_upgrades_pending Apt packages pending updates by origin.
# TYPE apt_upgrades_pending gauge
apt_upgrades_pending{arch="all",origin="Debian:bookworm-security/stable-security"} 1
apt_upgrades_pending{arch="amd64",origin="Debian:bookworm-security/stable-security"} 16
# HELP go_goroutines Number of goroutines that currently exist.
# TYPE go_goroutines gauge
go_goroutines 8
)");
  
  REQUIRE(res.size() == 2);
  CHECK(res.begin()->first=="apt_upgrades_pending");
  map<string,string> labels{{"arch", "amd64"}, {"origin", "Debian:bookworm-security/stable-security"}};
  CHECK(res["apt_upgrades_pending"].vals[labels].value == 16);
}

TEST_CASE("test with NaN") {
  PromParser p;
  auto res = p.parse(R"(# HELP go_memstats_mspan_sys_bytes Number of bytes used for mspan structures obtained from system.
# TYPE go_memstats_mspan_sys_bytes gauge
go_memstats_mspan_sys_bytes NaN
)");
  REQUIRE(res.size() == 1);
  map<string,string> emp;  
  CHECK(isnan(res["go_memstats_mspan_sys_bytes"].vals[emp].value) == 1);
}

TEST_CASE("test with large floating point") {
  PromParser p;
  auto res = p.parse(R"(# HELP go_memstats_alloc_bytes_total Total number of bytes allocated, even if freed.
# TYPE go_memstats_alloc_bytes_total counter
go_memstats_alloc_bytes_total 3.072603244608e+12
)");
  REQUIRE(res.size() == 1);
  map<string,string> emp;  
  CHECK(res["go_memstats_alloc_bytes_total"].vals[emp].value == 3.072603244608e+12);
  
}

TEST_CASE("test with escapes in labels") {
  PromParser p;
  auto res = p.parse(R"(# HELP go_gc_duration_seconds A summary of the pause duration of garbage collection cycles.
# TYPE go_gc_duration_seconds summary
go_gc_duration_seconds{quantile="0\n1\n\"2\""} 1.3045e-05 1713712554000
)");
  REQUIRE(res.size() == 1);
  map<string,string> emp;  
  CHECK(res["go_gc_duration_seconds"].vals.begin()->first.begin()->second == "0\n1\n\"2\"");
  
}

TEST_CASE("test with uncode") {
    PromParser p;
  auto res = p.parse(R"(# HELP apt_upgrades_held Apt packages pëndİng updates but held back.
# TYPE apt_upgrades_held gauge
apt_upgrades_held{arch="",origin=""} 0 1713712554000
)");
  REQUIRE(res.size() == 1);
  map<string,string> emp;  
  CHECK(res["apt_upgrades_held"].help == "Apt packages pëndİng updates but held back.");

}
