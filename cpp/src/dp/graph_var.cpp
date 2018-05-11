#include <stdlib.h>
#include <cxxabi.h>
#include <stdexcept>

#include "dp/graph.h"

namespace dp {
    using namespace std;

    graph::variable::variable(const string& name)
    : m_name(name), m_val(nullptr) {
    }

    graph::variable::~variable() {
        if (m_val) {
            delete m_val;
        }
    }

    void graph::variable::set_val(val_base *val) {
        clear();
        m_val = val;
    }

    void graph::variable::clear() {
        auto p = m_val;
        m_val = nullptr;
        if (p != nullptr) delete p;
    }

    graph::variable* graph::variable::must_set() {
        if (!is_set()) throw logic_error("variable not set: " + m_name);
        return this;
    }

    const graph::variable* graph::variable::must_set() const {
        return const_cast<graph::variable*>(this)->must_set();
    }

    static string demangle(const string& str) {
        string result;
        int status = 0;
        auto p = abi::__cxa_demangle(str.c_str(), nullptr, nullptr, &status);
        if (p != nullptr) {
            result = p;
            free(p);
        }
        return result;
    }

    graph::variable::type_error::type_error(const string& name,
        const string& actual_type, const string& desired_type)
    : logic_error("invalid cast var " + name + " from " + demangle(actual_type) + " to " + demangle(desired_type)),
      m_var_name(name), m_actual_type(actual_type), m_desired_type(desired_type) {
    }

    string graph::variable::type_error::actual_type_demangled() const {
        return demangle(m_actual_type);
    }

    string graph::variable::type_error::desired_type_demangled() const {
        return demangle(m_desired_type);
    }
}
