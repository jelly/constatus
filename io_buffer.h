// (C) 2017 by folkert van heusden, released under AGPL v3.0
#ifndef __IO_BUFFER_H__
#define __IO_BUFFER_H__

#include <string>

class io_buffer
{
private:
	unsigned char *pb; // push back
	int pb_len;
	//
	int fd;

public:
	io_buffer(int fd);
	~io_buffer();
	void push_back(unsigned char *what, int what_len);
	void push_back(char *what, int what_len) { push_back((unsigned char *)what, what_len); }

	unsigned int get_n_available() const { return pb_len; }

	bool read_available(unsigned char *where_to, int *where_to_len, int max_len);
	bool read_available(char *where_to, int *where_to_len, int max_len) { return read_available((unsigned char *)where_to, where_to_len, max_len); }

	std::string * read_line(unsigned int max_len, bool strip_cr);
};

#endif
