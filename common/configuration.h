/**
 * configuration.h
 * @author wherewindblow
 * @date   Oct 20, 2018
 */

#pragma once

#include <Poco/Util/AbstractConfiguration.h>


namespace spaceless {

namespace details {

/**
 * Expose getRaw, setRaw and enumerate for Configuration use.
 */
class JsonExposedConfiguration;

} // namespace details


/**
 * Configuration support operating multiply configuration file in once operation.
 * To create configuration can pass path list as argument to read multiply file.
 * All get operation will traverse all file in path list to get value.
 * It can get value will return immediately, or go on next to get value.
 */
class Configuration: public Poco::Util::AbstractConfiguration
{
public:
	using ExposedConfiguration = details::JsonExposedConfiguration;
	using PathList = std::vector<std::string>;

	Configuration() = default;

	/**
	 * Creates configuration by path.
	 */
	Configuration(const std::string& path);

	/**
	 * Creates configuration by path list.
	 * @note First path will high priority that second path.
	 */
	Configuration(const PathList& path_list);

	/**
	 * Destroys configuration.
	 */
	~Configuration();

	/**
	 * Loads configuration by path.
	 */
	void load(const std::string& path);

	/**
	 * Loads configuration by path list.
	 * @note First path will high priority that second path.
	 */
	void load(const PathList& path_list);

protected:
	bool getRaw(const std::string& key, std::string& value) const;

	void setRaw(const std::string& key, const std::string& value);

	void enumerate(const std::string& key, Keys& range) const;

private:
	std::vector<ExposedConfiguration*> m_cfg_list;
};

} // namespace spaceless
