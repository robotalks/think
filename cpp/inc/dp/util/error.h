#ifndef __DP_UTIL_ERROR_H
#define __DP_UTIL_ERROR_H

#include <string>

namespace dp {
    ::std::string _error_msg_at(const char* fn, int line, const ::std::string& prefix);
}

#define errmsg(prefix) ::dp::_error_msg_at(__FILE__, __LINE__, prefix)

#endif
