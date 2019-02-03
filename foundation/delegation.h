/**
 * delegate.h
 * @author wherewindblow
 * @date   Feb 02, 2019
 */

#pragma once

#include <functional>


namespace spaceless {

class Delegation
{
public:
	enum ThreadTarget
	{
		NETWORK,
		WORKER,
	};

	/**
	 * Let network thread to run delegate function.
	 * @note 1. @c func cannot capture any structure or class by reference in worker thread.
	 *       2. Cannot transfer any structure or class by reference to worker thread.
	 */
	static void delegate(ThreadTarget target, std::function<void()> func);
};

} // namespace spaceless
