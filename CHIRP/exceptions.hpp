#include <exception>
#include <string>

namespace cnstln {
namespace CHIRP {

class DecodeError : public std::exception {
public:
    DecodeError(std::string error_message) : error_message_(std::move(error_message)) {}
    const char* what() const noexcept override { return error_message_.c_str(); }
protected:
    std::string error_message_;
};

}
}
