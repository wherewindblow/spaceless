/**
 * log.cpp
 * @author wherewindblow
 * @date   Nov 05, 2017
 */

#include "log.h"
#include <lights/log_sinks/stdout_sink.h>

namespace spaceless {

lights::TextLogger logger("spaceless", lights::log_sinks::StdoutSink::instance());

} // namespace spaceless