#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "exec.h"
#include "parse.h"
#include "util.h"

namespace lang {
namespace parse {

std::set<std::string> Parser::reserved_idents = {
    "And",
    "As",
    "Boolean",
    "ByRef",
    "ByVal",
    "Dim",
    "End",
    "False",
    "Function",
    "If",
    "Integer",
    "Not",
    "Or",
    /* "String" not included because it is also a builtin. */
    "Then",
    "True",
    "While",
};

Parser::Parser(exec::Interpreter &interp, const std::string &src)
    : interp_(interp), line_num_(0)
{
    std::size_t start = 0;
    while (true) {
        auto nl_pos = src.find('\n', start);
        auto end = nl_pos == std::string::npos ? src.size() : nl_pos;
        lines_.emplace_back(src, start, end - start);
        if (nl_pos == std::string::npos)
            break;
        start = nl_pos + 1;
    }
}

exec::Context Parser::parse_globals(exec::Context *parent) {
    int saved_line_num = line_num_;

    exec::Context ctx(parent);

    for (int i = 0; i < lines_.size(); i++) {
        line_num_ = i + 1;
        buffer_ = lines_[i];

        if (!accept_bol())
            continue;

        if (!accept_dim(ctx))  {
            auto func_decl = accept_func_decl();
            if (func_decl.name_.empty())
                continue;
            int func_end = find_block_end(
                "Function", i + 1, static_cast<int>(lines_.size() - 1), false);
            auto func = std::make_unique<exec::ScriptFunction>(
                func_decl.name_, i + 1, func_end, func_decl.ret_type_);
            for (const auto &param : func_decl.params_)
                func->add_param(param.name_, param.type_, param.is_ref_);
            ctx.var(func_decl.name_, true)->set(std::move(func));
            i = func_end;
        }

        expect_eol();
    }

    line_num_ = saved_line_num;

    return ctx;
}

std::unique_ptr<objects::Object>
Parser::run_block(exec::Context &ctx, int start_line, int end_line, const std::string &ret_ident) {
    std::string_view saved_buffer = buffer_;
    int saved_line_num = line_num_;

    std::unique_ptr<objects::Object> retval;
    for (int i = start_line; i < end_line; i++) {
        line_num_ = i + 1;
        buffer_ = lines_[i];

        if (!accept_bol())
            continue;

        bool is_while = false;
        auto cond = accept_if_block(ctx);
        if (!cond.has_value()) {
            is_while = true;
            cond = accept_while_block(ctx);
        }
        if (cond.has_value()) {
            expect_eol();
            auto block_end = find_block_end(is_while ? "While" : "If", i + 1, end_line - 1);
            if (cond.value()) {
                auto block_retval = run_block(ctx, i + 1, block_end, ret_ident);
                if (block_retval)
                    retval = std::move(block_retval);
            }
            i = is_while && cond.value() ? i - 1 : block_end;
            continue;
        }

        auto stmt_retval = expect_stmt(ctx, ret_ident);
        if (stmt_retval)
            retval = std::move(stmt_retval);
        expect_eol();
    }

    buffer_ = saved_buffer;
    line_num_ = saved_line_num;

    return retval;
}

bool Parser::accept(const char *s) {
    if (!buffer_.starts_with(s))
        return false;
    buffer_.remove_prefix(std::strlen(s));
    return true;
}

void Parser::expect(const char *s) {
    using namespace std::string_literals;
    if (!accept(s))
        throw std::runtime_error("expected \""s + s + "\"");
}

bool Parser::accept_whitespace() {
    if (buffer_.empty())
        return false;
    auto pos = buffer_.find_first_not_of("\t\n\r ");
    if (pos == 0)
        return false;
    if (pos == std::string_view::npos)
        pos = buffer_.size();
    buffer_.remove_prefix(pos);
    return true;
}

void Parser::expect_whitespace() {
    if (!accept_whitespace())
        throw std::runtime_error("expected space");
}

bool Parser::accept_bol() {
    accept_whitespace();
    return !buffer_.empty() && !buffer_.starts_with('\'');
}

void Parser::expect_eol() {
    accept_whitespace();
    if (!buffer_.empty() && !buffer_.starts_with('\''))
        throw std::runtime_error("expected end of line");
}

bool Parser::accept_keyword(const char *keyword) {
    auto backtrack = buffer_;
    if (!accept(keyword))
        return false;
    if (buffer_.empty())
        return true;
    if (buffer_.find_first_not_of(
            "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_") == 0)
        return true;
    buffer_ = backtrack;
    return false;
}

void Parser::expect_keyword(const char *keyword) {
    using namespace std::string_literals;
    if (!accept_keyword(keyword))
        throw std::runtime_error("expected \""s + keyword + "\"");
}

std::optional<std::int64_t> Parser::accept_number() {
    std::size_t end;
    auto value = util::str_to_int(buffer_, &end);
    if (end == 0)
        return {};
    buffer_.remove_prefix(end);
    return value;
}

std::int64_t Parser::expect_number() {
    auto value = accept_number();
    if (!value)
        throw std::runtime_error("expected number");
    return value.value();
}

std::optional<std::string> Parser::accept_string_literal() {
    if (!accept("\""))
        return {};

    std::string str;
    bool escape = false;
    bool terminated = false;
    for (; !terminated && !buffer_.empty(); buffer_.remove_prefix(1)) {
        auto ch = buffer_[0];

        if (escape) {
            if (ch == 't')
                ch = '\t';
            else if (ch == 'n')
                ch = '\n';
            else if (ch == 'r')
                ch = '\r';
        } else {
            if (ch == '\\') {
                escape = true;
                continue;
            }
            if (ch == '"') {
                terminated = true;
                continue;
            }
        }

        str.push_back(ch);
        escape = false;
    }

    if (!terminated)
        throw std::runtime_error("unterminated string literal");

    return str;
}

std::string Parser::accept_ident() {
    auto pos = buffer_.find_first_of(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_");
    if (pos != 0)
        return {};
    pos = buffer_.find_first_not_of(
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_", pos);
    if (pos == std::string_view::npos)
        pos = buffer_.size();
    std::string ident{buffer_.substr(0, pos)};
    if (reserved_idents.find(ident) != reserved_idents.end())
        return {};
    buffer_.remove_prefix(pos);
    return ident;
}

std::string Parser::expect_ident() {
    auto ident = accept_ident();
    if (ident.empty())
        throw std::runtime_error("expected identifier");
    return ident;
}

objects::Type Parser::expect_type() {
    if (accept_keyword("Boolean"))
        return objects::Type::BOOLEAN;
    if (accept_keyword("Integer"))
        return objects::Type::INTEGER;
    if (accept_keyword("String"))
        return objects::Type::STRING;
    throw std::runtime_error("expected type");
}

Parser::VarDecl Parser::expect_var_decl(bool is_arg) {
    auto name = expect_ident();

    bool is_array = false;
    std::int64_t array_size = 0;
    bool need_ws = !accept_whitespace();
    if (accept("(")) {
        is_array = true;
        accept_whitespace();
        if (!is_arg)
            array_size = expect_number();
        accept_whitespace();
        expect(")");
        need_ws = true;
    }
    if (need_ws)
        expect_whitespace();

    expect_keyword("As");
    expect_whitespace();
    auto type = expect_type();

    if (is_array) {
        if (type == objects::Type::INTEGER)
            type = objects::Type::INTEGER_ARRAY;
        else
            throw std::runtime_error("invalid array type");
    }

    return {std::move(name), type, array_size, false};
}

bool Parser::accept_dim(exec::Context &ctx) {
    if (!accept_keyword("Dim"))
        return false;
    expect_whitespace();
    auto decl = expect_var_decl();
    ctx.var(decl.name_, true)->set(objects::create_default(decl.type_, decl.array_size_));
    return true;
}

Parser::VarDecl Parser::expect_param_decl() {
    bool is_ref = false;
    if (accept_keyword("ByRef")) {
        is_ref = true;
        expect_whitespace();
    } else if (accept_keyword("ByVal")) {
        expect_whitespace();
    }
    VarDecl decl = expect_var_decl(true);
    decl.is_ref_ = is_ref;
    return decl;
}

Parser::FuncDecl Parser::accept_func_decl() {
    if (!accept_keyword("Function"))
        return {};

    expect_whitespace();
    auto name = expect_ident();

    accept_whitespace();
    expect("(");
    accept_whitespace();

    std::vector<VarDecl> params;
    if (!accept(")")) {
        while (true) {
            params.push_back(expect_param_decl());
            accept_whitespace();
            if (!accept(","))
                break;
            accept_whitespace();
        }
        accept_whitespace();
        expect(")");
    }

    accept_whitespace();
    expect_keyword("As");
    expect_whitespace();
    auto ret_type = expect_type();

    return {std::move(name), std::move(params), ret_type};
}

Parser::LValue Parser::accept_subscript(exec::Context &ctx) {
    auto backtrack = buffer_;

    auto ident = accept_ident();
    if (ident.empty())
        return {};

    accept_whitespace();
    if (!accept("(")) {
        buffer_ = backtrack;
        return {};
    }

    auto arr = ctx.var(ident, false, true);
    if (!arr || !arr->is_int_array()) {
        buffer_ = backtrack;
        return {};
    }

    accept_whitespace();
    auto index_obj = expect_expr(ctx);
    accept_whitespace();
    expect(")");

    if (!index_obj->is_int())
        throw std::runtime_error("array index is not an integer");
    auto idx = index_obj->as_int()->value_;

    if (idx < 0 || static_cast<std::size_t>(idx) >= arr->as_int_array()->length_)
        throw std::out_of_range("array index out of bounds");

    return {std::move(ident), index_obj->as_int()->value_};
}

Parser::LValue Parser::accept_lvalue(exec::Context &ctx) {
    auto subscript = accept_subscript(ctx);
    if (!subscript.ident_.empty())
        return subscript;
    return {accept_ident(), {}};
}

std::optional<std::vector<std::unique_ptr<objects::Object>>>
Parser::accept_call_args(exec::Context &ctx) {
    if (!accept("("))
        return {};

    accept_whitespace();
    std::vector<std::unique_ptr<objects::Object>> args;
    if (!accept(")")) {
        while (true) {
            args.push_back(expect_expr(ctx));
            accept_whitespace();
            if (!accept(","))
                break;
            accept_whitespace();
        }
        accept_whitespace();
        expect(")");
    }

    return args;
}

std::unique_ptr<objects::Object>
Parser::accept_factor(exec::Context &ctx, const LValue *prefix) {
    auto &lv = prefix ? *prefix : accept_lvalue(ctx);
    if (!lv.ident_.empty()) {
        if (lv.index_.has_value()) {
            auto arr = ctx.var(lv.ident_)->as_int_array();
            auto int_value = arr->at(lv.index_.value());
            return std::make_unique<objects::Integer>(int_value);
        }
        auto backtrack = buffer_;
        accept_whitespace();
        auto args = accept_call_args(ctx);
        if (args.has_value())
            return interp_.call(lv.ident_, args.value());
        buffer_ = backtrack;
        return ctx.var(lv.ident_);
    }

    if (accept_keyword("True"))
        return std::make_unique<objects::Boolean>(true);
    if (accept_keyword("False"))
        return std::make_unique<objects::Boolean>(false);

    auto number = accept_number();
    if (number.has_value())
        return std::make_unique<objects::Integer>(number.value());

    auto str = accept_string_literal();
    if (str.has_value())
        return std::make_unique<objects::String>(str.value());

    if (accept("-")) {
        accept_whitespace();
        auto value = expect_factor(ctx);
        return interp_.unop_neg(value.get());
    }

    if (accept("(")) {
        accept_whitespace();
        auto value = expect_expr(ctx);
        accept_whitespace();
        expect(")");
        return value;
    }

    return {};
}

std::unique_ptr<objects::Object>
Parser::expect_factor(exec::Context &ctx, const LValue *prefix) {
    auto value = accept_factor(ctx, prefix);
    if (!value)
        throw std::runtime_error("expected factor");
    return value;
}

std::unique_ptr<objects::Object>
Parser::accept_term(exec::Context &ctx, const LValue *prefix) {
    auto lhs = accept_factor(ctx, prefix);
    if (!lhs)
        return {};

    while (true) {
        auto backtrack = buffer_;
        accept_whitespace();
        if (accept("*")) {
            accept_whitespace();
            auto rhs = expect_factor(ctx);
            lhs = interp_.binop_mul(lhs.get(), rhs.get());
        } else if (accept("/")) {
            accept_whitespace();
            auto rhs = expect_factor(ctx);
            lhs = interp_.binop_div(lhs.get(), rhs.get());
        } else {
            buffer_ = backtrack;
            break;
        }
    }

    return lhs;
}

std::unique_ptr<objects::Object>
Parser::expect_term(exec::Context &ctx, const LValue *prefix) {
    auto value = accept_term(ctx, prefix);
    if (!value)
        throw std::runtime_error("expected term");
    return value;
}

std::unique_ptr<objects::Object>
Parser::accept_arith_expr(exec::Context &ctx, const LValue *prefix) {
    auto lhs = accept_term(ctx, prefix);
    if (!lhs)
        return {};

    while (true)  {
        auto backtrack = buffer_;
        accept_whitespace();
        if (accept("+")) {
            accept_whitespace();
            auto rhs = expect_term(ctx);
            lhs = interp_.binop_add(lhs.get(), rhs.get());
        } else if (accept("-")) {
            accept_whitespace();
            auto rhs = expect_term(ctx);
            lhs = interp_.binop_sub(lhs.get(), rhs.get());
        } else if (accept("&")) {
            accept_whitespace();
            auto rhs = expect_term(ctx);
            lhs = interp_.binop_cat(lhs.get(), rhs.get());
        } else {
            buffer_ = backtrack;
            break;
        }
    }

    return lhs;
}

std::unique_ptr<objects::Object>
Parser::expect_arith_expr(exec::Context &ctx, const LValue *prefix) {
    auto value = accept_arith_expr(ctx, prefix);
    if (!value)
        throw std::runtime_error("expected expression (arith)");
    return value;
}

std::unique_ptr<objects::Object>
Parser::accept_cmp_expr(exec::Context &ctx, const LValue *prefix) {
    auto lhs = accept_arith_expr(ctx, prefix);
    if (!lhs)
        return {};

    auto backtrack = buffer_;
    accept_whitespace();

    if (accept("<>")) {
        accept_whitespace();
        auto rhs = expect_arith_expr(ctx);
        return interp_.binop_ne(lhs.get(), rhs.get());
    }

    if (accept("=")) {
        accept_whitespace();
        auto rhs = expect_arith_expr(ctx);
        return interp_.binop_eq(lhs.get(), rhs.get());
    }

    if (accept("<")) {
        bool eq = accept("=");
        accept_whitespace();
        auto rhs = expect_arith_expr(ctx);
        return eq ?
            interp_.binop_le(lhs.get(), rhs.get()) :
            interp_.binop_lt(lhs.get(), rhs.get());
    }

    if (accept(">")) {
        bool eq = accept("=");
        accept_whitespace();
        auto rhs = expect_arith_expr(ctx);
        return eq ?
            interp_.binop_ge(lhs.get(), rhs.get()) :
            interp_.binop_gt(lhs.get(), rhs.get());
    }

    buffer_ = backtrack;
    return lhs;
}

std::unique_ptr<objects::Object>
Parser::expect_cmp_expr(exec::Context &ctx, const LValue *prefix) {
    auto value = accept_cmp_expr(ctx, prefix);
    if (!value)
        throw std::runtime_error("expected expression (cmp)");
    return value;
}

std::unique_ptr<objects::Object>
Parser::accept_not_expr(exec::Context &ctx, const LValue *prefix) {
    if ((!prefix || prefix->ident_.empty()) && accept_keyword("Not")) {
        accept_whitespace();
        auto value = expect_cmp_expr(ctx);
        return interp_.unop_not(value.get());
    }
    return accept_cmp_expr(ctx, prefix);
}

std::unique_ptr<objects::Object>
Parser::expect_not_expr(exec::Context &ctx, const LValue *prefix) {
    auto value = accept_not_expr(ctx, prefix);
    if (!value)
        throw std::runtime_error("expected expression (not)");
    return value;
}

std::unique_ptr<objects::Object>
Parser::accept_and_expr(exec::Context &ctx, const LValue *prefix) {
    auto lhs = accept_not_expr(ctx, prefix);
    if (!lhs)
        return {};

    while (true) {
        auto backtrack = buffer_;
        accept_whitespace();
        if (accept_keyword("And")) {
            accept_whitespace();
            auto rhs = expect_not_expr(ctx);
            lhs = interp_.binop_and(lhs.get(), rhs.get());
        } else {
            buffer_ = backtrack;
            break;
        }
    }

    return lhs;
}

std::unique_ptr<objects::Object>
Parser::expect_and_expr(exec::Context &ctx, const LValue *prefix) {
    auto value = accept_and_expr(ctx, prefix);
    if (!value)
        throw std::runtime_error("expected expression (and)");
    return value;
}

std::unique_ptr<objects::Object>
Parser::accept_expr(exec::Context &ctx, const LValue *prefix) {
    auto lhs = accept_and_expr(ctx, prefix);
    if (!lhs)
        return {};

    /* Account an extra operation for each expression.
     * This makes it impossible to create infinite loops. */
    interp_.account_op();

    while (true)  {
        auto backtrack = buffer_;
        accept_whitespace();
        if (accept_keyword("Or")) {
            accept_whitespace();
            auto rhs = expect_and_expr(ctx);
            lhs = interp_.binop_or(lhs.get(), rhs.get());
        } else {
            buffer_ = backtrack;
            break;
        }
    }

    return lhs;
}

std::unique_ptr<objects::Object>
Parser::expect_expr(exec::Context &ctx, const LValue *prefix) {
    auto value = accept_expr(ctx, prefix);
    if (!value)
        throw std::runtime_error("expected expression");
    return value;
}

std::unique_ptr<objects::Object>
Parser::expect_stmt(exec::Context &ctx, const std::string &ret_ident) {
    if (accept_dim(ctx))
        return {};

    auto lv = accept_lvalue(ctx);
    if (!lv.ident_.empty()) {
        accept_whitespace();
        if (accept("=")) {
            accept_whitespace();
            auto value = expect_expr(ctx);
            if (lv.index_.has_value()) {
                if (!value->is_int())
                    throw std::runtime_error("expected integer value for array element");
                auto arr = ctx.var(lv.ident_)->as_int_array();
                arr->at(lv.index_.value()) = value->as_int()->value_;
                return {};
            }
            if (lv.ident_ == ret_ident)
                return value->is_ref() ? value->as_ref()->obj_->clone() : std::move(value);
            ctx.var(lv.ident_)->set(std::move(value));
            return {};
        }
    }

    if (!accept_expr(ctx, &lv))
        throw std::runtime_error("expected statement");

    return {};
}

std::optional<bool> Parser::accept_if_block(exec::Context &ctx) {
    if (!accept_keyword("If"))
        return {};

    expect_whitespace();
    auto cond = expect_expr(ctx);
    expect_whitespace();
    expect_keyword("Then");

    if (!cond->is_bool())
        throw std::runtime_error("If condition must be a Boolean");

    return cond->as_bool()->value_;
}

std::optional<bool> Parser::accept_while_block(exec::Context &ctx) {
    if (!accept_keyword("While"))
        return {};

    expect_whitespace();
    auto cond = expect_expr(ctx);

    if (!cond->is_bool())
        throw std::runtime_error("While condition must be a Boolean");

    return cond->as_bool()->value_;
}

int Parser::find_block_end(const char *keyword, int line_min, int line_max,
                           bool nestable) {
    using namespace std::string_literals;

    auto saved_buffer = buffer_;
    auto saved_line_num = line_num_;

    for (int i = line_min; i <= line_max; i++) {
        line_num_ = i + 1;
        buffer_ = lines_[i];

        if (!accept_bol())
            continue;

        if (accept_keyword(keyword)) {
            if (!nestable)
                throw std::runtime_error("cannot nest "s + keyword + " blocks");
            i = find_block_end(keyword, i+1, line_max);
            continue;
        }

        if (!accept_keyword("End"))
            continue;
        if (!accept_whitespace())
            continue;
        if (!accept_keyword(keyword))
            continue;

        expect_eol();

        buffer_ = saved_buffer;
        line_num_ = saved_line_num;
        return i;
    }

    throw std::runtime_error("expected end of "s + keyword + " block");
}

} // namespace parse
} // namespace lang
