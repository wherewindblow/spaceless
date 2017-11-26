/**
 * log.h
 * @author wherewindblow
 * @date   Nov 05, 2017
 */

#pragma once

#include <lights/logger.h>
#include <lights/log_sinks/stdout_sink.h>

namespace spaceless {

extern lights::TextLogger logger;

#define SPACELESS_DEBUG(module, ...) \
	LIGHTS_DEBUG(spaceless::logger, module, __VA_ARGS__);
#define SPACELESS_INFO(module, ...) \
	LIGHTS_INFO(spaceless::logger, module, __VA_ARGS__);
#define SPACELESS_WARN(module, ...) \
	LIGHTS_WARN(spaceless::logger, module, __VA_ARGS__);
#define SPACELESS_ERROR(module, ...) \
	LIGHTS_ERROR(spaceless::logger, module, __VA_ARGS__);

} // namespace spaceless
