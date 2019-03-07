/**
 * exception.h
 * @author wherewindblow
 * @date   Nov 18, 2017
 */

#pragma once

#include <lights/exception.h>
#include <lights/format.h>

namespace spaceless {

using lights::Exception;

bool safe_call(std::function<void()> function, lights::TextWriter& error_msg, int* error_code = nullptr);

} // namespace spaceless
