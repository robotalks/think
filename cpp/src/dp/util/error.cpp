#include <unistd.h>
#include <errno.h>
#include <cstring>
#include "dp/util/error.h"

namespace dp {
    using namespace std;

    string _error_msg_at(const char* fn, int line, const string& prefix) {
        char loc[32], buf[32];
        sprintf(loc, ":%d: ", line);
        sprintf(buf, ": (%d) ", errno);
        return string(fn) + string(loc) + prefix + string(buf) + string(strerror(errno));
    }
}
