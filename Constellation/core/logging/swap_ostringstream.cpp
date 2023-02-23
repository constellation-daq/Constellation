#include "Constellation/core/logging/swap_ostringstream.hpp"

#include "Constellation/core/logging/Logger.hpp"

using namespace Constellation;

swap_ostringstream::~swap_ostringstream() {
    swap(logger_->os_);
    logger_->flush();
}
