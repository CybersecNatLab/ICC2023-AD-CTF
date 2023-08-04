#ifndef PARSE_H
#define PARSE_H

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "objects.h"

namespace lang {

namespace exec {
class Context;
class Interpreter;
}

namespace parse {

class Parser {
public:
    Parser(exec::Interpreter &interp, const std::string &src);

    exec::Context parse_globals(exec::Context *parent = nullptr);
    std::unique_ptr<objects::Object> run_block(
        exec::Context &ctx, int start_line, int end_line, const std::string &ret_ident = {});

    int line_num() { return line_num_; }
    std::string buffer() { return std::string(buffer_); }

private:
    static std::set<std::string> reserved_idents;

    struct VarDecl {
        std::string name_;
        objects::Type type_;
        std::int64_t array_size_;
        bool is_ref_;
    };

    struct FuncDecl {
        std::string name_;
        std::vector<VarDecl> params_;
        objects::Type ret_type_;
    };

    struct LValue {
        std::string ident_;
        std::optional<std::size_t> index_;
    };

    bool accept(const char *s);
    void expect(const char *s);
    bool accept_whitespace();
    void expect_whitespace();
    bool accept_bol();
    void expect_eol();
    bool accept_keyword(const char *keyword);
    void expect_keyword(const char *keyword);
    std::optional<std::int64_t> accept_number();
    std::int64_t expect_number();
    std::optional<std::string> accept_string_literal();
    std::string accept_ident();
    std::string expect_ident();
    objects::Type expect_type();
    VarDecl expect_var_decl(bool is_arg = false);
    bool accept_dim(exec::Context &ctx);
    VarDecl expect_param_decl();
    FuncDecl accept_func_decl();
    LValue accept_subscript(exec::Context &ctx);
    LValue accept_lvalue(exec::Context &ctx);
    std::optional<std::vector<std::unique_ptr<objects::Object>>>
        accept_call_args(exec::Context &ctx);
    std::unique_ptr<objects::Object> accept_factor(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> expect_factor(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> accept_term(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> expect_term(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> accept_arith_expr(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> expect_arith_expr(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> accept_cmp_expr(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> expect_cmp_expr(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> accept_not_expr(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> expect_not_expr(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> accept_and_expr(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> expect_and_expr(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> accept_or_expr(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> expect_or_expr(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> accept_expr(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> expect_expr(
        exec::Context &ctx, const LValue *prefix = nullptr);
    std::unique_ptr<objects::Object> expect_stmt(
        exec::Context &ctx, const std::string &ret_ident = {});
    std::optional<bool> accept_if_block(exec::Context &ctx);
    std::optional<bool> accept_while_block(exec::Context &ctx);

    int find_block_end(const char *keyword, int line_min, int line_max,
                       bool nestable = true);

    exec::Interpreter &interp_;
    std::vector<std::string> lines_;
    std::string_view buffer_;
    int line_num_;
};

} // namespace parse
} // namespace lang

#endif // PARSE_H
