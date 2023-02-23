#pragma once

#include <sstream>

namespace Constellation {
    // Forward declaration of Logger class
    class Logger;

    // Class that swap content with Logger stream and calls Logger::flush
    class swap_ostringstream : public std::ostringstream {
    public:
        swap_ostringstream(Logger* logger) : logger_(logger) {}

        virtual ~swap_ostringstream();

    private:
        Logger* logger_;
    };
}
