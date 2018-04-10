#ifndef BYTE_ARRAY_JSON_EXCEPTIONS
#define BYTE_ARRAY_JSON_EXCEPTIONS

#include "byte_array/referenced_byte_array.hpp"
#include "byte_array/tokenizer/token.hpp"

#include <exception>
#include <sstream>
namespace fetch {
namespace json {

class UnrecognisedJSONSymbolException : public std::exception {
  std::string str_;

 public:
  UnrecognisedJSONSymbolException(byte_array::Token const& token) {
    std::stringstream msg;
    msg << "Unrecognised symbol '";
    msg << token << '\'' << " at line " << token.line() << ", character "
        << token.character() << "\n";
    str_ = msg.str();
  }

  virtual const char* what() const throw() { return str_.c_str(); }
};

  class JSONParseException : public std::exception {
  public:
    JSONParseException(std::string const& err) : error_(err) { }
    JSONParseException(std::string const& err, byte_array::Token const &t) : error_(err), token_(t) { }    
    virtual ~JSONParseException() {}
    virtual char const * what() const throw() {
      return error_.c_str();
    }
  private:
    std::string error_;
    byte_array::Token token_;
  };

  
};
};
#endif
