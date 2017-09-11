#ifndef __META_H__
#define __META_H__

#include <map>
#include <mutex>
#include <string>

class meta
{
private:
	std::mutex m_int_lock;
	std::map<std::string, std::pair<uint64_t, int> > m_int;
	std::mutex m_double_lock;
	std::map<std::string, std::pair<uint64_t, double> > m_double;
	std::mutex m_string_lock;
	std::map<std::string, std::pair<uint64_t, std::string> > m_string;

public:
	meta();
	~meta();

	std::pair<uint64_t, int> getInt(const std::string & key);
	void setInt(const std::string & key, const std::pair<uint64_t, int> & v);

	std::pair<uint64_t, double> getDouble(const std::string & key);
	void setDouble(const std::string & key, const std::pair<uint64_t, double> & v);

	std::pair<uint64_t, std::string> getString(const std::string & key);
	void setString(const std::string & key, const std::pair<uint64_t, std::string> & v);
};

#endif
