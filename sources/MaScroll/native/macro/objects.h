#ifndef OBJECTS_H
#define OBJECTS_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace lang {

namespace exec { class Interpreter; }

namespace objects {

enum Type {
    REFERENCE,
    BOOLEAN,
    INTEGER,
    INTEGER_ARRAY,
    STRING,
    FUNCTION,
};

const char *type_to_str(Type type);

void track_alloc(std::size_t size);
void track_dealloc(std::size_t size);

struct Reference;
struct Boolean;
struct Integer;
struct IntegerArray;
struct String;
struct Function;

struct Object {
    virtual ~Object() = default;
    virtual std::unique_ptr<Object> clone() = 0;
    virtual std::string describe() = 0;

    bool is_ref() { return type_ == Type::REFERENCE; }
    bool is_bool() { return unref()->type_ == BOOLEAN; }
    bool is_int() { return unref()->type_ == INTEGER; }
    bool is_int_array() { return unref()->type_ == INTEGER_ARRAY; }
    bool is_str() { return unref()->type_ == STRING; }
    bool is_func() { return unref()->type_ == FUNCTION; }

    Reference *as_ref();
    Boolean *as_bool();
    Integer *as_int();
    IntegerArray *as_int_array();
    String *as_str();
    Function *as_func();

    Object *unref();

    Type type_;

protected:
    Object(Type type) : type_(type) {}
};

std::unique_ptr<Object> create_default(Type type, std::int64_t array_size = 0);

struct Reference : public Object {
    Reference(std::unique_ptr<Object> &obj);
    Reference(const Reference &);
    ~Reference();

    std::unique_ptr<Object> clone() override;
    std::string describe() override;

    void set(std::unique_ptr<Object> value);

    std::unique_ptr<Object> &obj_;
};

struct Boolean : public Object {
    Boolean(bool value);
    Boolean(const Boolean &);
    ~Boolean();

    std::unique_ptr<Object> clone() override;
    std::string describe() override;

    bool value_;
};

struct Integer : public Object {
    Integer(std::int64_t value);
    Integer(const Integer &);
    ~Integer();

    std::unique_ptr<Object> clone() override;
    std::string describe() override;

    std::int64_t value_;
};

struct IntegerArray : public Object {
    IntegerArray(const IntegerArray &) = delete;
    ~IntegerArray();

    IntegerArray &operator=(const IntegerArray &) = delete;
    void operator delete(void *ptr);
    std::unique_ptr<Object> clone() override;
    std::string describe() override;
    std::int64_t &at(std::int64_t idx);

    static std::unique_ptr<IntegerArray> create(std::int64_t length);

    /* Must be first to overlap with String::length_ */
    std::size_t length_;
private:
    std::int64_t values_[1];

    IntegerArray(std::size_t length);
};

struct String : public Object {
    String(const std::string &str);
    String(const String &);
    ~String();

    String &operator=(const String &);
    std::unique_ptr<Object> clone() override;
    std::string describe() override;
    std::string to_std();

    /* Must be first to overlap with IntegerArray::length_ */
    std::size_t length_;
    char *str_;
};

struct Function : public Object {
    static const std::size_t MAX_PARAMS = 64;

    Function(const std::string &name) : Object(FUNCTION), name_(name), ref_bitmap_(0) {};
    virtual std::unique_ptr<Object> call(
        exec::Interpreter &interp, std::vector<std::unique_ptr<Object>> &args) = 0;

    void set_ref_param(std::size_t idx) { ref_bitmap_ |= std::uint64_t(1) << idx; }
    bool is_ref_param(std::size_t idx) { return ref_bitmap_ & (std::uint64_t(1) << idx); }

    std::string name_;
    std::uint64_t ref_bitmap_;
};

} // namespace objects
} // namespace lang

#endif // OBJECTS_H
