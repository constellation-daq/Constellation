#pragma once

#include <exception>
#include <string>

namespace cnstln {
namespace CHIRP {

/**
 * Error thrown when a Message was not decoded successfully
*/
class DecodeError : public std::exception {
public:
    /**
     * @param error_message Error message
    */
    DecodeError(std::string error_message) : error_message_(std::move(error_message)) {}
    /**
     * @return Error message
    */
    const char* what() const noexcept final { return error_message_.c_str(); }
protected:
    /** The error message */
    std::string error_message_;
};

}
}
