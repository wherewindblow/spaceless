/**
 * configuration.cpp
 * @author wherewindblow
 * @date   Oct 20, 2018
 */


#include "configuration.h"

#include <Poco/Util/JSONConfiguration.h>
#include <lights/env.h>
#include <lights/file.h>


namespace spaceless {

namespace details {

class JsonExposedConfiguration: public Poco::Util::JSONConfiguration
{
public:
	using Base = Poco::Util::JSONConfiguration;
	using Base::getRaw;
	using Base::setRaw;
	using Base::enumerate;

	JsonExposedConfiguration() = default;

	JsonExposedConfiguration(const std::string& path):
		Base(path) {}
};

} // namespace details


Configuration::Configuration(const std::string& path)
{
	load(path);
}


Configuration::Configuration(const Configuration::PathList& path_list)
{
	load(path_list);
}


Configuration::~Configuration()
{
	for (auto cfg : m_cfg_list)
	{
		delete cfg;
	}
}


void Configuration::load(const std::string& path)
{
	m_cfg_list.push_back(new ExposedConfiguration(path));
}


void Configuration::load(const Configuration::PathList& path_list)
{
	for (auto& path : path_list)
	{
		if (lights::env::file_exists(path.c_str()))
		{
			m_cfg_list.push_back(new ExposedConfiguration(path));
		}
	}
}


bool Configuration::getRaw(const std::string& key, std::string& value) const
{
	for (auto cfg : m_cfg_list)
	{
		if (cfg->getRaw(key, value))
		{
			return true;
		}
	}
	return false;
}


void Configuration::setRaw(const std::string& key, const std::string& value)
{
	for (auto cfg : m_cfg_list)
	{
		cfg->setRaw(key, value); // Set first cfg.
		return;
	}
}


void Configuration::enumerate(const std::string& key, Configuration::Keys& range) const
{
	for (auto cfg : m_cfg_list)
	{
		cfg->enumerate(key, range); // Merge all enumerate range.
	}
}

} // namespace spaceless
