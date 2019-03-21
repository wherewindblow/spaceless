/**
 * delegate.h
 * @author wherewindblow
 * @date   Feb 02, 2019
 */

#pragma once

#include <functional>
#include <lights/sequence.h>


namespace spaceless {

class Delegation
{
public:
	enum ActorTarget
	{
		NETWORK,
		WORKER,
	};

	/**
	 * Let target actor to run delegate function.
	 * @param actor         Specify the thread to run function.
	 * @param function      Function that do something.
	 * @param caller        Indicates who call this delegate.
	 * @note 1. @c function cannot capture any structure or class by reference in current thread.
	 *       2. Cannot transfer any structure or class by reference to current thread.
	 *       3. @c caller lifecycle must be ensure by caller.
	 * Why use @c lights::SourceLocation to instead of @c where, because lambda cannot capture
	 * more than two argument when use macro to expend current source location with
	 * void delegate(ThreadTarget thread_target, std::function<void()> func, const lights::SourceLocation where).
	 */
	static void delegate(lights::StringView caller, ActorTarget actor, std::function<void()> function);
};

} // namespace spaceless
