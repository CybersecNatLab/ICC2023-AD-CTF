#include <cstddef>
#include <stdexcept>

#include "exec.h"

/* Maximum allowed operations. */
#define MAX_OPS 384

namespace lang {
namespace exec {

std::unique_ptr<objects::Reference>
Context::var(const std::string &name, bool define, bool nullable) {
    auto var = find_var(name, !define);
    if (define) {
        if (var)
            throw std::runtime_error("redefined identifier " + name);
        return std::make_unique<objects::Reference>(vars_[name]);
    } else {
        if (!var) {
            if (!nullable)
                throw std::runtime_error("undefined identifier " + name);
            return {};
        }
        auto obj = var->get();
        return std::make_unique<objects::Reference>(
            obj->is_ref() ? obj->as_ref()->obj_ : *var);
    }
}

std::unique_ptr<objects::Object> *Context::find_var(const std::string &name, bool recurse) {
    auto it = vars_.find(name);
    if (it != vars_.end())
        return &it->second;
    if (!recurse || !parent_)
        return nullptr;
    return parent_->find_var(name);
}

Interpreter::Interpreter(const std::string &src, Context *root_ctx)
    : parser_(*this, src), num_ops_(0)
{
    try {
        globals_ = parser_.parse_globals(root_ctx);
    } catch (const std::exception &exc) {
        throw std::runtime_error(
            "line " + std::to_string(parser_.line_num()) + ": " + exc.what() +
            ": near \"" + parser_.buffer() + "\"");
    }
}

std::string Interpreter::run(const std::string &input)
{
    using namespace std::string_literals;

    std::vector<std::unique_ptr<objects::Object>> main_args;
    main_args.push_back(std::move(std::make_unique<objects::String>(input)));

    std::unique_ptr<objects::Object> retval;
    try {
        retval = call("Main", main_args);
    } catch (const std::exception &exc) {
        throw std::runtime_error(
            "line " + std::to_string(parser_.line_num()) + ": " + exc.what() +
            ": near \"" + parser_.buffer() + "\"");
    }

    if (!retval->is_str())
        throw std::runtime_error(
            "Main returned "s + objects::type_to_str(retval->unref()->type_) +
            ", expected string");
    return retval->as_str()->to_std();
}

std::unique_ptr<objects::Object>
Interpreter::call(const std::string &name, std::vector<std::unique_ptr<objects::Object>> &args) {
    account_op();

    auto &value = globals_.var(name)->obj_;
    if (!value->is_func())
        throw std::runtime_error(name + " is not a function");
    auto func = value->as_func();

    if (args.size() > objects::Function::MAX_PARAMS)
        throw std::runtime_error("too many arguments in call to " + func->name_);

    std::vector<std::unique_ptr<objects::Object>> new_args;
    for (std::size_t i = 0; i < args.size(); i++) {
        auto &arg = args[i];
        bool is_ref = arg->is_ref();
        bool need_ref = func->is_ref_param(i);
        if (need_ref && !is_ref)
            new_args.push_back(std::make_unique<objects::Reference>(arg));
        else if (is_ref && !need_ref)
            new_args.push_back(arg->as_ref()->obj_->clone());
        else
            new_args.push_back(std::move(arg));
    }

    return func->call(*this, new_args);
}

std::unique_ptr<objects::Object> Interpreter::unop_neg(objects::Object *obj) {
    using namespace std::string_literals;
    account_op();
    if (obj->is_int())
        return std::make_unique<objects::Integer>(-obj->as_int()->value_);
    throw std::runtime_error(
        "invalid type for unary - operator: "s +
        objects::type_to_str(obj->unref()->type_));
}

std::unique_ptr<objects::Object> Interpreter::unop_not(objects::Object *obj) {
    using namespace std::string_literals;
    account_op();
    if (obj->is_bool())
        return std::make_unique<objects::Boolean>(!obj->as_bool()->value_);
    if (obj->is_int())
        return std::make_unique<objects::Integer>(static_cast<std::int64_t>(
            ~static_cast<std::uint64_t>(obj->as_int()->value_)));
    throw std::runtime_error(
        "invalid type for unary Not operator: "s +
        objects::type_to_str(obj->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_add(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_int() && rhs->is_int())
        return std::make_unique<objects::Integer>(
            lhs->as_int()->value_ + rhs->as_int()->value_);
    throw std::runtime_error(
        "invalid types for binary + operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_sub(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_int() && rhs->is_int())
        return std::make_unique<objects::Integer>(
            lhs->as_int()->value_ - rhs->as_int()->value_);
    throw std::runtime_error(
        "invalid types for binary - operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_mul(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_int() && rhs->is_int())
        return std::make_unique<objects::Integer>(
            lhs->as_int()->value_ * rhs->as_int()->value_);
    throw std::runtime_error(
        "invalid types for binary * operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_div(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_int() && rhs->is_int()) {
        auto rhs_value = rhs->as_int()->value_;
        if (rhs_value == 0)
            throw std::runtime_error("division by zero");
        return std::make_unique<objects::Integer>(
            lhs->as_int()->value_ / rhs_value);
    }
    throw std::runtime_error(
        "invalid types for binary / operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_cat(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_str() && rhs->is_str())
        return std::make_unique<objects::String>(
            lhs->as_str()->to_std() + rhs->as_str()->to_std());
    throw std::runtime_error(
        "invalid types for binary & operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_and(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_bool() && rhs->is_bool())
        return std::make_unique<objects::Boolean>(
            lhs->as_bool()->value_ && rhs->as_bool()->value_);
    if (lhs->is_int() && rhs->is_int())
        return std::make_unique<objects::Integer>(static_cast<std::int64_t>(
            static_cast<std::uint64_t>(lhs->as_int()->value_) &
            static_cast<std::uint64_t>(rhs->as_int()->value_)));
    throw std::runtime_error(
        "invalid types for binary And operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_or(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_bool() && rhs->is_bool())
        return std::make_unique<objects::Boolean>(
            lhs->as_bool()->value_ || rhs->as_bool()->value_);
    if (lhs->is_int() && rhs->is_int())
        return std::make_unique<objects::Integer>(static_cast<std::int64_t>(
            static_cast<std::uint64_t>(lhs->as_int()->value_) |
            static_cast<std::uint64_t>(rhs->as_int()->value_)));
    throw std::runtime_error(
        "invalid types for binary Or operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_lt(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_int() && rhs->is_int())
        return std::make_unique<objects::Boolean>(
            lhs->as_int()->value_ < rhs->as_int()->value_);
    if (lhs->is_str() && rhs->is_str())
        return std::make_unique<objects::Boolean>(
            lhs->as_str()->to_std() < rhs->as_str()->to_std());
    throw std::runtime_error(
        "invalid types for binary < operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_le(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_int() && rhs->is_int())
        return std::make_unique<objects::Boolean>(
            lhs->as_int()->value_ <= rhs->as_int()->value_);
    if (lhs->is_str() && rhs->is_str())
        return std::make_unique<objects::Boolean>(
            lhs->as_str()->to_std() <= rhs->as_str()->to_std());
    throw std::runtime_error(
        "invalid types for binary <= operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_gt(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_int() && rhs->is_int())
        return std::make_unique<objects::Boolean>(
            lhs->as_int()->value_ > rhs->as_int()->value_);
    if (lhs->is_str() && rhs->is_str())
        return std::make_unique<objects::Boolean>(
            lhs->as_str()->to_std() > rhs->as_str()->to_std());
    throw std::runtime_error(
        "invalid types for binary > operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_ge(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_int() && rhs->is_int())
        return std::make_unique<objects::Boolean>(
            lhs->as_int()->value_ >= rhs->as_int()->value_);
    if (lhs->is_str() && rhs->is_str())
        return std::make_unique<objects::Boolean>(
            lhs->as_str()->to_std() >= rhs->as_str()->to_std());
    throw std::runtime_error(
        "invalid types for binary >= operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_eq(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_int() && rhs->is_int())
        return std::make_unique<objects::Boolean>(
            lhs->as_int()->value_ == rhs->as_int()->value_);
    if (lhs->is_str() && rhs->is_str())
        return std::make_unique<objects::Boolean>(
            lhs->as_str()->to_std() == rhs->as_str()->to_std());
    throw std::runtime_error(
        "invalid types for binary = operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

std::unique_ptr<objects::Object>
Interpreter::binop_ne(objects::Object *lhs, objects::Object *rhs) {
    using namespace std::string_literals;
    account_op();
    if (lhs->is_int() && rhs->is_int())
        return std::make_unique<objects::Boolean>(
            lhs->as_int()->value_ != rhs->as_int()->value_);
    if (lhs->is_str() && rhs->is_str())
        return std::make_unique<objects::Boolean>(
            lhs->as_str()->to_std() != rhs->as_str()->to_std());
    throw std::runtime_error(
        "invalid types for binary <> operator: "s +
        objects::type_to_str(lhs->unref()->type_) + " and " +
        objects::type_to_str(rhs->unref()->type_));
}

void Interpreter::account_op() {
    if (++num_ops_ >= MAX_OPS)
        throw std::runtime_error("operation limit exceeded");
}

std::unique_ptr<objects::Object> ScriptFunction::clone() {
    return std::make_unique<ScriptFunction>(*this);
}

std::string ScriptFunction::describe() {
    return "[ScriptFunction " + name_ + "]";
}

std::unique_ptr<objects::Object>
ScriptFunction::call(Interpreter &interp, std::vector<std::unique_ptr<Object>> &args) {
    if (args.size() != params_.size())
        throw std::runtime_error(
            name_ + " received " + std::to_string(args.size()) +
            " arguments, expected " + std::to_string(params_.size()));

    Context ctx(&interp.globals());

    for (std::size_t i = 0; i < args.size(); i++) {
        auto &param = params_[i];
        auto obj = std::move(args[i]);

        auto obj_type = obj->unref()->type_;
        if (obj_type != param.type_)
            throw std::runtime_error(
                name_ + " received parameter " + std::to_string(i+1) +
                " of type " + objects::type_to_str(obj_type) +
                ", expected "  + objects::type_to_str(param.type_));

        /* If we used ->set(), reference parameters would be copied. */
        ctx.var(param.name_, true)->obj_ = std::move(obj);
    }

    auto retval = interp.parser().run_block(ctx, start_line_, end_line_, name_);
    if (!retval)
        throw std::runtime_error(name_ + " did not return a value");
    if (retval->type_ != ret_type_)
        throw std::runtime_error(
            name_ + " returned " + objects::type_to_str(retval->type_) +
            ", expected " + objects::type_to_str(ret_type_));

    return retval;
}

void ScriptFunction::add_param(const std::string &name, objects::Type type, bool is_ref) {
    if (params_.size() == objects::Function::MAX_PARAMS)
        throw std::runtime_error("too many function parameters");
    params_.emplace_back(name, type);
    if (is_ref)
        set_ref_param(params_.size() - 1);
}

} // namespace exec
} // namespace lang
