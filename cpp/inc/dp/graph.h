#ifndef __DP_GRAPH_H
#define __DP_GRAPH_H

#include <memory>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <stdexcept>
#include <typeinfo>

namespace dp {

    class graph {
    private:
        struct op;

    public:
        class variable {
        public:
            struct val_base {
                virtual ~val_base() {}
                virtual ::std::string type() const = 0;
            };

            variable(const ::std::string& name);
            virtual ~variable();

            const ::std::string& name() const { return m_name; }
            bool is_set() const { return m_val != nullptr; }
            void set_val(val_base*);
            val_base* val() const { return m_val; }
            void clear();

            variable* must_set();
            const variable* must_set() const;

        private:
            ::std::string m_name;
            val_base* m_val;

        public:
            class type_error : public ::std::logic_error {
            public:
                type_error(const ::std::string& var_name,
                          const ::std::string& actual_type,
                          const ::std::string& desired_type);

                const ::std::string& var_name() const { return m_var_name; }
                const ::std::string& actual_type() const { return m_actual_type; }
                const ::std::string& desired_type() const { return m_desired_type; }

                ::std::string actual_type_demangled() const;
                ::std::string desired_type_demangled() const;

            private:
                ::std::string m_var_name;
                ::std::string m_actual_type;
                ::std::string m_desired_type;
            };

            template<typename T>
            T& as() const {
                auto p = dynamic_cast<graph::val<T>*>(const_cast<variable*>(must_set())->m_val);
                if (p == nullptr) {
                    throw type_error(m_name, m_val->type(), graph::val<T>().type());
                }
                return p->value;
            }

            template<typename T>
            void set(const T& v) {
                auto p = new graph::val<T>();
                p->value = v;
                set_val(p);
            }

            template<typename T>
            void set(T&& v) {
                auto p = new graph::val<T>();
                p->value = ::std::move(v);
                set_val(p);
            }
        };

        template<typename T>
        struct val : public variable::val_base {
            T value;
            virtual ::std::string type() const {
                return typeid(val).name();
            }
        };

        class ctx {
        public:
            const ::std::string& name() const { return m_op->name; }
            const ::std::vector<variable*>& in() const { return m_op->params; }
            const ::std::vector<variable*>& out() const { return m_op->results; }

            variable* in(size_t at) const { return in()[at]; }
            variable* out(size_t at) const { return out()[at]; }

            ::std::function<void()> defer();

        private:
            const op *m_op;
            ::std::function<void()> *m_done_ptr;

            ctx(op *op, ::std::function<void()> *done_ptr)
            : m_op(op), m_done_ptr(done_ptr) { }

            friend class op;
        };

        using op_func = ::std::function<void(ctx)>;

        graph(const ::std::string& name = ::std::string());

        const ::std::string& name() const { return m_name; }

        variable* def_var(const ::std::string& name);
        void def_vars(const ::std::vector<::std::string>& names);
        void add_op(const ::std::string& name,
            const ::std::vector<::std::string>& inputs,
            const ::std::vector<::std::string>& outputs,
            const op_func& fn);

        variable* find_var(const ::std::string& name) const noexcept;
        variable* var(const ::std::string& name) const;
        void reset();
        void exec(size_t concurrency = 4);

    private:
        struct op {
            ::std::string name;
            op_func fn;
            ::std::vector<variable*> params;
            ::std::vector<variable*> results;
            bool activated;

            op(const ::std::string& _name, const op_func& _fn)
            : name(_name), fn(_fn), activated(false) { }

            bool ready() const {
                for (auto& v : params) {
                    if (!v->is_set()) return false;
                }
                return true;
            }

            void run(::std::function<void()>);
        };

        ::std::string m_name;

        ::std::unordered_map<::std::string, ::std::unique_ptr<variable> > m_vars;
        ::std::unordered_map<::std::string, ::std::unique_ptr<op> > m_ops;
        ::std::unordered_map<::std::string, ::std::string> m_out_vars;
    };
}

#endif
