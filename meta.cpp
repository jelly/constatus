#include "meta.h"

meta::meta()
{
}

meta::~meta()
{
}

std::pair<uint64_t, int> meta::getInt(const std::string & key)
{
	std::pair<uint64_t, int> res;

	m_int_lock.lock();

	auto it = m_int.find(key);

	if (it == m_int.end())
		res.first = res.second = 0;
	else
		res = it -> second;

	m_int_lock.unlock();

	return res;
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

	if (it == m_double.end()) {
		res.first = 0;
		res.second = 0.0;
	}
	else {
		res = it -> second;
	}

	m_double_lock.unlock();

	return res;
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

	if (it == m_string.end())
		res.first = 0;
	else
		res = it -> second;

	m_string_lock.unlock();

	return res;
}

void meta::setString(const std::string & key, const std::pair<uint64_t, std::string> & v)
{
	m_string_lock.lock();
	m_string.erase(key);
	m_string.insert(std::pair<std::string, std::pair<uint64_t, std::string> >(key, v));
	m_string_lock.unlock();
}
