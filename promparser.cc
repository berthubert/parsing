#include "promparser.hh"
#include "peglib.h"
#include <fmt/ranges.h>
using namespace std;
  
PromParser::PromParser()
{
  d_p = std::make_unique<peg::parser>();
  auto& p = *d_p; // saves bit of typing

  d_p->set_logger([](size_t line, size_t col, const string& msg, const string &rule) {
    fmt::print("line {}, col {}: {}\n", line, col,msg, rule);
  }); // gets us some helpful errors if the grammar is wrong
  
  auto ok = d_p->load_grammar(R"(
root          <- ( ( commentline / vline ) '\n')+
commentline   <- ('# HELP ' name ' ' comment) /
                 ('# TYPE ' name ' ' comment) /
                 ('#' comment)
comment       <- (!'\n' .)*
vline         <- (name ' ' value (' ' timestamp)?)  /
                 (name labels ' ' value (' ' timestamp)?) 
name          <- [a-zA-Z0-9_]+ 
labels        <- '{' nvpair (',' nvpair)* '}' 
nvpair        <- name '=' '"' label_value '"' 
label_value   <- (!'"' char)*
char          <- ('\\' . ) /
                 (!'\\' .)
value         <- '+Inf' / '-Inf' / 'NaN' / [0-9.+e-]+ 
timestamp     <- [+-]?[0-9]*
)" );

  if(!ok) 
    throw runtime_error("Error in grammar\n");

  // this creates an attractive error messsage in case of a problem, and stores it
  // so we can throw a useful exception later if parsing fails
  d_p->set_logger([this](size_t line, size_t col, const string& msg) {
    d_error = fmt::format("Error on line {}:{} -> {}", line, col, msg);
  });

  // This contains a comment line, where choice 0 is "HELP", choice 1 is "TYPE"
  // choice 2 is random comment, which we ignore
  struct CommentLine
  {
    size_t choice;
    string name;
    string comment;
  };
  // here we parse a comment line, and return a CommentLine
  p["commentline"] = [](const peg::SemanticValues &vs) {
    if(vs.choice() == 0) 
      return CommentLine({vs.choice(), std::any_cast<string>(vs[0]), std::any_cast<string>(vs[1])});
    else if(vs.choice() == 1) 
      return CommentLine({vs.choice(), std::any_cast<string>(vs[0]), std::any_cast<string>(vs[1])});

    return CommentLine({vs.choice(), string(), string()});
  };

  // this merely returns the comment contents as a string
  p["comment"] = [](const peg::SemanticValues &vs) {
     return vs.token_to_string();
  };
  // and similar for the 'name' rule
  p["name"] = [](const peg::SemanticValues &vs) {
    return vs.token_to_string();
  };
  // this is where deal with the un-escaping, using the choice()
  p["char"] = [](const peg::SemanticValues &vs) {
    string res = vs.token_to_string();
    if(vs.choice() == 0) { // this was an escape
      char c = res.at(1);
      if(c != '\\' && c != 'n' && c != '"')
	throw runtime_error(fmt::format("Unknown escape sequence '\\{}'", c));
      if(c == 'n') 
	c = '\n';
      return c;
    }
    return res.at(0);
  };
  // here we assemble all the "char"'s from above into a label_value string
  p["label_value"] = [](const peg::SemanticValues &vs) {
    string ret;
    for(const auto& v : vs)
      ret.append(1, std::any_cast<char>(v));
    return ret;
  };
  // only invoked if a timestamp was passed
  p["timestamp"] = [](const peg::SemanticValues &vs) {
    return vs.token_to_number<int64_t>();
  };
  // combines a label key="value" pair into a std::pair<string,string>
  p["nvpair"] = [](const peg::SemanticValues &vs) {
    return std::make_pair(std::any_cast<string>(vs[0]), std::any_cast<string>(vs[1]));
  };
  // gathers all these pairs into a map<string,string>
  p["labels"] = [](const peg::SemanticValues &vs) {
    map<string,string> m;
    for(const auto& sel : vs) {
      const auto p = std::any_cast<pair<string,string>>(sel);
      m.insert(p);
    }
    return m;
  };
  // detects if a numerical value is perhaps +Inf, -Inf, NaN or a floating point value
  p["value"] = [](const peg::SemanticValues &vs) {
    if(vs.choice() == 0 )       // +Inf
      return numeric_limits<double>::infinity();
    else if(vs.choice() == 1 )  // -Inf
      return -numeric_limits<double>::infinity();
    else if(vs.choice() == 2 )  // NaN
      return numeric_limits<double>::quiet_NaN();
    
    return vs.token_to_number<double>();
  };
  // this reflects the contents of a vline
  struct VlineDetails
  {
    string name;
    map<string, string> labels;
    double value;
    int64_t tstampmsec = 0;
  };

  /* Deals with the two choices, a line witout/without labels
  vline         <- (name ' ' value (' ' timestamp)?)  /
                   (name labels ' ' value (' ' timestamp)?) 
  */
  
  p["vline"] = [](const peg::SemanticValues &vs) {
    VlineDetails d;
    unsigned int pos = 0;
    d.name = std::any_cast<string>(vs[pos++]);

    if(vs.choice() == 1) {
      d.labels = std::any_cast<decltype(d.labels)>(vs[pos++]);
    }
    d.value = std::any_cast<double>(vs[pos++]);

    if(pos < vs.size()) {
      d.tstampmsec = std::any_cast<int64_t>(vs[pos++]);
    }
    return d;
  };
  // this is the first rule, and the one that ::parse will return
  // root consists of an array of VlineDetails and CommentLines
  // which we join together in the promparseres_t map
  p["root"] = [](const peg::SemanticValues &vs)  {
    promparseres_t ret;
    for(const auto& v : vs) {
      if(auto dptr = std::any_cast<VlineDetails>(&v)) {
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

PromParser::~PromParser(){} // needed -here- because of std::unique_ptr<>
