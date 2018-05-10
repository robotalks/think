#include <stdlib.h>
#include <cxxabi.h>
#include <stdexcept>

#include "vp/graph.h"

namespace vp {
    using namespace std;

    Graph::Var::Var(const string& name)
    : m_name(name), m_val(nullptr) {
    }

    Graph::Var::~Var() {
        if (m_val) {
            delete m_val;
        }
    }

    void Graph::Var::set_val(Val *val) {
        clear();
        m_val = val;
    }

    void Graph::Var::clear() {
        auto p = m_val;
        m_val = nullptr;
        if (p != nullptr) delete p;
    }

    Graph::Var* Graph::Var::must_set() {
        if (!is_set()) throw logic_error("variable not set: " + m_name);
        return this;
    }

    const Graph::Var* Graph::Var::must_set() const {
        return const_cast<Var*>(this)->must_set();
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

    Graph::Var::TypeError::TypeError(const string& name,
        const string& actual_type, const string& desired_type)
    : logic_error("invalid cast var " + name + " from " + demangle(actual_type) + " to " + demangle(desired_type)),
      m_var_name(name), m_actual_type(actual_type), m_desired_type(desired_type) {
    }

    string Graph::Var::TypeError::actual_type_demangled() const {
        return demangle(m_actual_type);
    }

    string Graph::Var::TypeError::desired_type_demangled() const {
        return demangle(m_desired_type);
    }
}
