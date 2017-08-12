#include <stdlib.h>
#include <string>
#include <unistd.h>

void exec(const std::string & what, const std::string & parameter)
{
	unshare(CLONE_FILES);

	if (!what.empty() && fork() == 0) {
		system((what + " " + parameter).c_str());

		exit(0);
	}
}
