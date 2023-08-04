#ifndef BUILTINS_H
#define BUILTINS_H

#include <Windows.h>
#include <bcrypt.h>
#include <memory>
#include <string>
#include <vector>

#include "exec.h"
#include "objects.h"

namespace lang {
namespace builtins {

struct NativeFunction : public objects::Function {
    using FuncT = std::unique_ptr<Object> (*)(
        exec::Interpreter &, std::vector<std::unique_ptr<Object>> &);

    NativeFunction(const std::string &name, FuncT func) : Function(name), func_(func) {}

    std::unique_ptr<Object> clone() override;
    std::string describe() override;
    std::unique_ptr<Object> call(
        exec::Interpreter &interp, std::vector<std::unique_ptr<Object>> &args) override;

    FuncT func_;
};

struct RNGFunction : public objects::Function {
    using GenT = void (*)(void *, std::size_t);

    RNGFunction(const std::string &name, GenT gen);

    std::unique_ptr<Object> clone() override;
    std::string describe() override;
    std::unique_ptr<Object> call(
        exec::Interpreter &interp, std::vector<std::unique_ptr<Object>> &args) override;

    GenT gen_;
};

class SignTokenFunction : public objects::Function {
public:
    SignTokenFunction(const std::string &name, const std::string &key_path);

    std::unique_ptr<Object> clone() override;
    std::string describe() override;
    std::unique_ptr<Object> call(
        exec::Interpreter &interp, std::vector<std::unique_ptr<Object>> &args) override;

private:
    std::size_t num_calls_;
    BCRYPT_HASH_HANDLE hash_;
    BCRYPT_KEY_HANDLE key_;
};

void register_native(exec::Context &ctx, const std::string &name, NativeFunction::FuncT func);
void register_rng(exec::Context &ctx, const std::string &name, RNGFunction::GenT gen);
void register_builtins(exec::Context &ctx);

} // namespace builtins
} // namespace lang

#endif // BUILTINS_H
