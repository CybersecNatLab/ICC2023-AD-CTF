#ifndef EXEC_H
#define EXEC_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "objects.h"
#include "parse.h"

namespace lang {
namespace exec {

class Context {
public:
    Context(Context *parent = nullptr) : parent_(parent) {}

    std::unique_ptr<objects::Reference> var(const std::string &name, bool define = false,
                                            bool nullable = false);

private:
    std::unique_ptr<objects::Object> *find_var(const std::string &name, bool recurse = true);

    Context *parent_;
    std::unordered_map<std::string, std::unique_ptr<objects::Object>> vars_;
};

class Interpreter {
public:
    Interpreter(const std::string &src, Context *root_ctx = nullptr);
    std::string run(const std::string &input);
    std::unique_ptr<objects::Object> call(
        const std::string &name, std::vector<std::unique_ptr<objects::Object>> &args);
    std::unique_ptr<objects::Object> unop_neg(objects::Object *obj);
    std::unique_ptr<objects::Object> unop_not(objects::Object *obj);
    std::unique_ptr<objects::Object> binop_add(objects::Object *lhs, objects::Object *rhs);
    std::unique_ptr<objects::Object> binop_sub(objects::Object *lhs, objects::Object *rhs);
    std::unique_ptr<objects::Object> binop_mul(objects::Object *lhs, objects::Object *rhs);
    std::unique_ptr<objects::Object> binop_div(objects::Object *lhs, objects::Object *rhs);
    std::unique_ptr<objects::Object> binop_cat(objects::Object *lhs, objects::Object *rhs);
    std::unique_ptr<objects::Object> binop_and(objects::Object *lhs, objects::Object *rhs);
    std::unique_ptr<objects::Object> binop_or(objects::Object *lhs, objects::Object *rhs);
    std::unique_ptr<objects::Object> binop_lt(objects::Object *lhs, objects::Object *rhs);
    std::unique_ptr<objects::Object> binop_le(objects::Object *lhs, objects::Object *rhs);
    std::unique_ptr<objects::Object> binop_gt(objects::Object *lhs, objects::Object *rhs);
    std::unique_ptr<objects::Object> binop_ge(objects::Object *lhs, objects::Object *rhs);
    std::unique_ptr<objects::Object> binop_eq(objects::Object *lhs, objects::Object *rhs);
    std::unique_ptr<objects::Object> binop_ne(objects::Object *lhs, objects::Object *rhs);

    void account_op();

    parse::Parser &parser() { return parser_; }
    Context &globals() { return globals_; }

private:
    parse::Parser parser_;
    Context globals_;
    size_t num_ops_;
};

struct ScriptFunction : public objects::Function {
    ScriptFunction(const std::string &name, int start_line, int end_line, objects::Type ret_type)
        : Function(name), start_line_(start_line), end_line_(end_line), ret_type_(ret_type) {}

    std::unique_ptr<Object> clone() override;
    std::string describe() override;
    std::unique_ptr<Object> call(
        Interpreter &interp, std::vector<std::unique_ptr<Object>> &args) override;

    void add_param(const std::string &name, objects::Type type, bool is_ref = false);

    struct Param {
        std::string name_;
        objects::Type type_;
    };

    int start_line_;
    int end_line_;
    std::vector<Param> params_;
    objects::Type ret_type_;
};

} // namespace exec
} // namespace lang

#endif // EXEC_H
