#ifndef __VP_GRAPH_H
#define __VP_GRAPH_H

#include <memory>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <stdexcept>
#include <typeinfo>

namespace vp {

    class Graph {
    private:
        struct Op;

    public:
        class Var {
        public:
            struct Val {
                virtual ~Val() {}
                virtual ::std::string type() const = 0;
            };

            Var(const ::std::string& name);
            virtual ~Var();

            const ::std::string& name() const { return m_name; }
            bool is_set() const { return m_val != nullptr; }
            void set_val(Val*);
            Val* val() const { return m_val; }
            void clear();

            Var* must_set();
            const Var* must_set() const;

        private:
            ::std::string m_name;
            Val* m_val;

        public:
            class TypeError : public ::std::logic_error {
            public:
                TypeError(const ::std::string& var_name,
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
                auto p = dynamic_cast<Graph::Val<T>*>(const_cast<Var*>(must_set())->m_val);
                if (p == nullptr) {
                    throw TypeError(m_name, m_val->type(), Graph::Val<T>().type());
                }
                return p->val;
            }

            template<typename T>
            void set(const T& v) {
                auto p = new Graph::Val<T>();
                p->val = v;
                set_val(p);
            }

            template<typename T>
            void set(T&& v) {
                auto p = new Graph::Val<T>();
                p->val = ::std::move(v);
                set_val(p);
            }
        };

        template<typename T>
        struct Val : public Var::Val {
            T val;
            virtual ::std::string type() const {
                return typeid(val).name();
            }
        };

        class Ctx {
        public:
            const ::std::string& name() const { return m_op->name; }
            const ::std::vector<Var*>& in() const { return m_op->params; }
            const ::std::vector<Var*>& out() const { return m_op->results; }

            Var* in(size_t at) const { return in()[at]; }
            Var* out(size_t at) const { return out()[at]; }

            ::std::function<void()> defer();

        private:
            const Op *m_op;
            ::std::function<void()> *m_done_ptr;

            Ctx(Op *op, ::std::function<void()> *done_ptr)
            : m_op(op), m_done_ptr(done_ptr) { }

            friend class Op;
        };

        using OpFunc = ::std::function<void(Ctx)>;

        Graph(const ::std::string& name = ::std::string());

        const ::std::string& name() const { return m_name; }

        Var* defVar(const ::std::string& name);
        void defVars(const ::std::vector<::std::string>& names);
        void addOp(const ::std::string& name,
            const ::std::vector<::std::string>& inputs,
            const ::std::vector<::std::string>& outputs,
            const OpFunc& fn);

        Var* findVar(const ::std::string& name) const noexcept;
        Var* var(const ::std::string& name) const;
        void reset();
        void exec(size_t concurrency = 4);

    private:
        struct Op {
            ::std::string name;
            OpFunc fn;
            ::std::vector<Var*> params;
            ::std::vector<Var*> results;

            size_t set_varn;

            Op(const ::std::string& _name, const OpFunc& _fn)
            : name(_name), fn(_fn), set_varn(0) { }

            bool activated() const { return set_varn == params.size(); }
            void run(::std::function<void()>);
        };

        ::std::string m_name;

        ::std::unordered_map<::std::string, ::std::unique_ptr<Var> > m_vars;
        ::std::unordered_map<::std::string, ::std::unique_ptr<Op> > m_ops;
        ::std::unordered_map<::std::string, ::std::string> m_out_vars;
        ::std::unordered_map<::std::string, ::std::list<Op*>> m_var_deps;

    public:
        using OpFactory = ::std::function<OpFunc(void)>;

        class Builder {
        public:
            Builder() {}
            virtual ~Builder() { }

            Builder& var(const ::std::string& name);
            Builder& vars(const ::std::vector<::std::string>& names);
            Builder& op(const ::std::string& name,
                const ::std::vector<::std::string>& inputs,
                const ::std::vector<::std::string>& outputs,
                const OpFactory& factory);

            Graph* build(const ::std::string& name = ::std::string()) const;

        protected:
            virtual Graph* newGraph(const ::std::string& name) const { return new Graph(name); }

        private:
            struct OpDef {
                ::std::string name;
                ::std::vector<::std::string> inputs;
                ::std::vector<::std::string> outputs;
                OpFactory factory;
            };

            ::std::unordered_set<::std::string> m_vars;
            ::std::unordered_map<::std::string, OpDef> m_ops;
            ::std::unordered_map<::std::string, ::std::string> m_out_vars;
        };
    };
}

#endif
