#ifndef __DP_GRAPH_DEF_H
#define __DP_GRAPH_DEF_H

#include <istream>
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>

#include "dp/graph.h"
#include "dp/ingress.h"

namespace dp {
    class graph_def {
    public:
        using params = ::std::unordered_map<::std::string, ::std::string>;

        struct op_factory {
            virtual ~op_factory() { }
            virtual graph::op_func create_op(
                const ::std::string& name,
                const ::std::string& type, const params&) = 0;
        };

        struct ingress_factory {
            virtual ~ingress_factory() { }
            virtual ingress* create_ingress(
                const ::std::string& name,
                const ::std::string& type, const params&) = 0;
        };

        class op_registry : public op_factory {
        public:
            op_registry();
            virtual ~op_registry() { }

            virtual graph::op_func create_op(
                const ::std::string& name,
                const ::std::string& type,
                const params&);

            void add_factory(const ::std::string& name, op_factory*);
            op_factory* get_factory(const ::std::string& name) const;

        private:
            ::std::unordered_map<::std::string, op_factory*> m_factories;
        };

        class ingress_registry : public ingress_factory {
        public:
            ingress_registry();
            virtual ~ingress_registry() { }

            virtual ingress* create_ingress(
                const ::std::string& name,
                const ::std::string& type,
                const params&);

            void add_factory(const ::std::string& name, ingress_factory*);
            ingress_factory* get_factory(const ::std::string& name) const;

        private:
            ::std::unordered_map<::std::string, ingress_factory*> m_factories;
        };

        struct location {
            size_t off;
            size_t line, col;

            location() : off(0), line(0), col(0) { }
            ::std::string prefix() const;
        };

        class parse_error : public ::std::runtime_error {
        public:
            parse_error(const location& loc, const ::std::string& msg)
            : ::std::runtime_error(loc.prefix() + " " + msg),
              m_loc(loc), m_msg(msg) {
            }

            const location& loc() const { return m_loc; }
            const ::std::string& message() const { return m_msg; }

        private:
            location m_loc;
            ::std::string m_msg;
        };

    public:
        graph_def();
        graph_def(op_registry *ops, ingress_registry *ins);

        virtual ~graph_def() { }

        graph_def& use_ops(op_registry *ops);
        graph_def& use_ingresses(ingress_registry *ins);

        graph_def& parse(::std::istream&);
        graph_def& parse(const ::std::string&);
        graph_def& parse_file(const ::std::string&);

        graph_def& select_graph(const ::std::string& name);

        exec_env& build_env(exec_env&, const params& args) const;

        ::std::vector<::std::string> graph_names() const;

        graph* build_graph(const params& args) const;
        graph* build_graph(const ::std::string& name, const params& args) const;

        ::std::list<::std::unique_ptr<ingress>> build_ingresses(const params& args) const;
        ::std::list<::std::unique_ptr<ingress>> build_ingresses_in_graph(
            const ::std::string& graph_name, const params& args) const;

    private:
        op_registry* m_ops;
        ingress_registry* m_ins;
        class xref;
        ::std::unique_ptr<xref> m_refs;
        ::std::string m_selected_graph;

        void build_xref(xref*);
    };
}

#endif
