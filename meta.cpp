#include <dlfcn.h>

#include "meta.h"
#include "utils.h"
#include "error.h"

meta::meta()
{
}

meta::~meta()
{
	for(meta_plugin_t & cur : plugins) {
		cur.uninit_plugin(cur.arg);
		dlclose(cur.handle);
	}
}

void meta::add_plugin(const std::string & plugin_filename, const std::string & parameter)
{
	meta_plugin_t mp;

	mp.handle = dlopen(plugin_filename.c_str(), RTLD_NOW);
	if (!mp.handle)
		error_exit(true, "Failed opening source plugin library %s: %s", plugin_filename.c_str(), dlerror());

	mp.init_plugin = (init_plugin_t)find_symbol(mp.handle, "init_plugin", "meta plugin", plugin_filename.c_str());
	mp.uninit_plugin = (uninit_plugin_t)find_symbol(mp.handle, "uninit_plugin", "meta plugin", plugin_filename.c_str());

	mp.arg = mp.init_plugin(this, parameter.c_str());

	plugins.push_back(mp);
}

std::pair<uint64_t, int> meta::getInt(const std::string & key)
{
	std::pair<uint64_t, int> res;

	m_int_lock.lock();

	auto it = m_int.find(key);

	if (it == m_int.end() || (it -> second.first < get_us() && it -> second.first != 0))
		res.first = res.second = 0;
	else
		res = it -> second;

	m_int_lock.unlock();

	return res;
}

bool meta::hasInt(const std::string & key)
{
	m_int_lock.lock();

	auto it = m_int.find(key);
	bool has_it = it != m_int.end();

	m_int_lock.unlock();

	return has_it;
}

void meta::setInt(const std::string & key, const std::pair<uint64_t, int> & v)
{
	m_int_lock.lock();
	m_int.erase(key);
	m_int.insert(std::pair<std::string, std::pair<uint64_t, int> >(key, v));
	m_int_lock.unlock();
}

std::pair<uint64_t, double> meta::getDouble(const std::string & key)
{
	std::pair<uint64_t, double> res;

	m_double_lock.lock();

	auto it = m_double.find(key);

	if (it == m_double.end() || (it -> second.first < get_us() && it -> second.first != 0)) {
		res.first = 0;
		res.second = 0.0;
	}
	else {
		res = it -> second;
	}

	m_double_lock.unlock();

	return res;
}

bool meta::hasDouble(const std::string & key)
{
	m_double_lock.lock();

	auto it = m_double.find(key);
	bool has_it = it != m_double.end();

	m_double_lock.unlock();

	return has_it;
}

void meta::setDouble(const std::string & key, const std::pair<uint64_t, double> & v)
{
	m_double_lock.lock();
	m_double.erase(key);
	m_double.insert(std::pair<std::string, std::pair<uint64_t, double> >(key, v));
	m_double_lock.unlock();
}

std::pair<uint64_t, std::string> meta::getString(const std::string & key)
{
	std::pair<uint64_t, std::string> res;

	m_string_lock.lock();

	auto it = m_string.find(key);

	if (it == m_string.end() || (it -> second.first < get_us() && it -> second.first != 0))
		res.first = 0;
	else
		res = it -> second;

	m_string_lock.unlock();

	return res;
}

bool meta::hasString(const std::string & key)
{
	m_string_lock.lock();

	auto it = m_string.find(key);
	bool has_it = it != m_string.end();

	m_string_lock.unlock();

	return has_it;
}

void meta::setString(const std::string & key, const std::pair<uint64_t, std::string> & v)
{
	m_string_lock.lock();
	m_string.erase(key);
	m_string.insert(std::pair<std::string, std::pair<uint64_t, std::string> >(key, v));
	m_string_lock.unlock();
}
