#pragma once

#include "Constellation/core/logging/log.hpp"
#include "Constellation/core/logging/Logger.hpp"

#define GEN_LOGGER_INSTANCE(class_name, logger_topic) \
    class class_name : public Constellation::Logger { \
    public: \
        static class_name& getInstance() { \
            static class_name instance {}; \
            return instance; \
        } \
        class_name(class_name const&) = delete; \
        class_name& operator=(class_name const&) = delete; \
    \
    private: \
        class_name() : Constellation::Logger(logger_topic) { \
            /* debug settings for now*/ \
            enableTrace(); \
            setConsoleLogLevel(TRACE); \
        } \
    }
