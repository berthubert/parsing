#include "promparser.hh"
#include "peglib.h"
#include <fmt/ranges.h>
using namespace std;

  
PromParser::PromParser()
{
  d_p = std::make_unique<peg::parser>();
  auto& p = *d_p;
  
  d_p->set_logger([](size_t line, size_t col, const string& msg, const string &rule) {
    fmt::print("line {}, col {}: {}\n", line, col,msg, rule);
  });
  
  auto ok = d_p->load_grammar(R"(
root          <- ( ( commentline / vline ) '\n')+
commentline   <- ('# HELP ' name ' ' comment) /
                 ('# TYPE ' name ' ' comment) /
                 ('#' comment)
comment       <- (!'\n' .)*
vline         <- (name ' ' value (' ' timestamp)?)  /
                 (name sels ' ' value (' ' timestamp)?) 
name          <- [a-zA-Z0-9_]+ 
sels          <- '{' nvpair (',' nvpair)* '}' 
nvpair        <- name '=' '"' label_value '"' 
label_value   <- (!'"' char)*
char          <- ('\\' . ) / (!'\\' .)
value         <- '+Inf' / '-Inf' / 'NaN' / [0-9.+e-]+ 
timestamp     <- [+-]?[1-9][0-9]*
)" );

  //  [-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)? 
  
  if(!ok) {
    throw runtime_error("Error in grammar\n");
  }
  d_p->set_logger([this](size_t line, size_t col, const string& msg) {
    d_error = fmt::format("Error on line {}:{} -> {}", line, col, msg);
  });

  struct CommentLine
  {
    size_t choice;
    string name;
    string comment;
  };
  
  p["commentline"] = [](const peg::SemanticValues &vs) {
    if(vs.choice() == 0) {
      return CommentLine({vs.choice(), std::any_cast<string>(vs[0]), std::any_cast<string>(vs[1])});
    }
    else if(vs.choice() == 1) {
      return CommentLine({vs.choice(), std::any_cast<string>(vs[0]), std::any_cast<string>(vs[1])});
    }
    return CommentLine({vs.choice(), string(), string()});
  };

  p["comment"] = [](const peg::SemanticValues &vs) {
     return vs.token_to_string();
  };

  p["name"] = [](const peg::SemanticValues &vs) {
    return vs.token_to_string();
  };

  p["char"] = [](const peg::SemanticValues &vs) {
    string res = vs.token_to_string();
    if(vs.choice() == 0) { // this was an escape
      char c = res.at(1);
      if(c != '\\' && c != 'n' && c!='"')
	throw runtime_error(fmt::format("Unknown escape sequence '\\{}'", c));
      if(c == 'n') {
	c = '\n';
      }
      return c;
    }
    return res.at(0);
  };

  p["label_value"] = [](const peg::SemanticValues &vs) {
    string ret;
    for(const auto& v : vs)
      ret.append(1, std::any_cast<char>(v));
    return ret;
  };

  p["timestamp"] = [](const peg::SemanticValues &vs) {
    return vs.token_to_number<int64_t>();
  };

  p["nvpair"] = [](const peg::SemanticValues &vs) {
    return std::make_pair(std::any_cast<string>(vs[0]), std::any_cast<string>(vs[1]));
  };

  p["sels"] = [](const peg::SemanticValues &vs) {
    map<string,string> m;
    for(const auto& sel : vs) {
      const auto p = std::any_cast<pair<string,string>>(sel);
      m.insert(p);
    }
    return m;
  };

  p["value"] = [](const peg::SemanticValues &vs) {
    if(vs.choice() == 0 )  // +Inf
      return numeric_limits<double>::infinity();
    else if(vs.choice() == 1 )  // -Inf
      return -numeric_limits<double>::infinity();
    else if(vs.choice() == 2 )  // NaN
      return numeric_limits<double>::quiet_NaN();
    
    return vs.token_to_number<double>();
  };

  struct Details
  {
    string name;
    map<string, string> labels;
    double value;
    int64_t tstampmsec = 0;
  };

  /*
  vline         <- (name ' ' value (' ' timestamp)?)  /
                   (name sels ' ' value (' ' timestamp)?) 
  */
  
  p["vline"] = [](const peg::SemanticValues &vs) {
    Details d;
    unsigned int pos = 0;
    d.name = std::any_cast<string>(vs[pos]);
    pos++;
    if(vs.choice() == 1) {
      d.labels = std::any_cast<decltype(d.labels)>(vs[pos]);
      pos++;
    }
    d.value = std::any_cast<double>(vs[pos]);
    pos++;
    if(pos < vs.size()) {
      d.tstampmsec = std::any_cast<int64_t>(vs[pos]);
    }
    return d;
  };


  p["root"] = [](const peg::SemanticValues &vs)  {
    promparseres_t ret;
    for(const auto& v : vs) {
      if(auto dptr = std::any_cast<Details>(&v)) {
	ret[dptr->name].vals[dptr->labels]={dptr->tstampmsec, dptr->value};
      }
      else if(auto cptr = std::any_cast<CommentLine>(&v)) {

	if(cptr->choice == 0)
	  ret[cptr->name].help = cptr->comment;
	else if(cptr->choice == 1)
	  ret[cptr->name].type = cptr->comment;
	// ignore random comments (choice == 2)
      }
    }
    return ret;
  };
}

PromParser::promparseres_t PromParser::parse(const std::string& in)
{
  PromParser::promparseres_t ret;
  if(!d_p->parse(in, ret))
    throw runtime_error("Unable to parse prometheus input: "+d_error);
  return ret;
}


PromParser::~PromParser()
{}
