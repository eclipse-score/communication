#include "score/someip/routing_module/iosock_transport/log.h"

#include "score/mw/log/logging.h"

#include <string_view>

extern "C" void someip_routing_log(const someip_log_severity severity, const char* const message)
{
    const std::string_view text{message};
    switch (severity)
    {
        case SOMEIP_LOG_ERROR:
            score::mw::log::LogError("SRTM") << text;
            break;
        case SOMEIP_LOG_WARNING:
            score::mw::log::LogWarn("SRTM") << text;
            break;
        case SOMEIP_LOG_INFO:
            score::mw::log::LogInfo("SRTM") << text;
            break;
        case SOMEIP_LOG_DEBUG:
        default:
            score::mw::log::LogDebug("SRTM") << text;
            break;
    }
}
