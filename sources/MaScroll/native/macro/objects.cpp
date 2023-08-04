#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <iostream>

#include "objects.h"

#define MAX_MEM_USE (128 << 10)

namespace lang {
namespace objects {

static std::size_t g_mem_use;

const char *type_to_str(Type type) {
    switch (type) {
    case REFERENCE:
        return "reference";
    case BOOLEAN:
        return "boolean";
    case INTEGER:
        return "integer";
    case INTEGER_ARRAY:
        return "array of integer";
    case STRING:
        return "string";
    case FUNCTION:
        return "function";
    default:
        return "???";
    }
}

void track_alloc(std::size_t size)
{
    g_mem_use += size;
    if (g_mem_use > MAX_MEM_USE)
        throw std::runtime_error("memory limit exceeded");
}

void track_dealloc(std::size_t size)
{
    g_mem_use -= size;
}

Reference *Object::as_ref() {
    return static_cast<Reference *>(this);
}

Boolean *Object::as_bool() {
    return static_cast<Boolean *>(unref());
}

Integer *Object::as_int() { 
    return static_cast<Integer *>(unref());
}

IntegerArray *Object::as_int_array() { 
    return static_cast<IntegerArray *>(unref());
}

String *Object::as_str() { 
    return static_cast<String *>(unref());
}

Function *Object::as_func() { 
    return static_cast<Function *>(unref());
}

Object *Object::unref() {
    return is_ref() ? as_ref()->obj_.get() : this;
}

std::unique_ptr<Object> create_default(Type type, std::int64_t array_size) {
    switch (type) {
    case BOOLEAN:
        return std::make_unique<Boolean>(false);
    case INTEGER:
        return std::make_unique<Integer>(0);
    case INTEGER_ARRAY:
        return IntegerArray::create(array_size);
    case STRING:
        return std::make_unique<String>("");
    default:
        std::cerr << "Internal error: invalid type for default construction\n";
        std::abort();
    }
}

Reference::Reference(std::unique_ptr<Object> &obj) : Object(REFERENCE), obj_(obj) {
    track_alloc(sizeof(Reference));
}

Reference::Reference(const Reference &other) : Object(REFERENCE), obj_(other.obj_) {
    track_alloc(sizeof(Reference));
}

Reference::~Reference() {
    track_dealloc(sizeof(Reference));
}

std::unique_ptr<Object> Reference::clone() {
    return std::make_unique<Reference>(*this);
}

std::string Reference::describe() {
    return "[Reference to " + obj_->describe() + "]";
}

void Reference::set(std::unique_ptr<Object> value) {
    using namespace std::string_literals;

    if (value->is_ref())
        value = value->as_ref()->obj_->clone();

    if (obj_ && obj_->type_ != value->type_)
        throw std::runtime_error(
            "assignment of value of type "s + objects::type_to_str(value->type_) +
            " to variable of type " + objects::type_to_str(obj_->type_));

    obj_ = std::move(value);
}

Boolean::Boolean(bool value) : Object(BOOLEAN), value_(value) {
    track_alloc(sizeof(Boolean));
}

Boolean::Boolean(const Boolean &other) : Object(BOOLEAN), value_(other.value_) {
    track_alloc(sizeof(Boolean));
}

Boolean::~Boolean() {
    track_dealloc(sizeof(Boolean));
}

std::unique_ptr<Object> Boolean::clone() {
    return std::make_unique<Boolean>(*this);
}

std::string Boolean::describe() {
    return "[Boolean]";
}

Integer::Integer(std::int64_t value) : Object(INTEGER), value_(value) {
    track_alloc(sizeof(Integer));
}

Integer::Integer(const Integer &other) : Object(INTEGER), value_(other.value_) {
    track_alloc(sizeof(Integer));
}

Integer::~Integer() {
    track_dealloc(sizeof(Integer));
}

std::unique_ptr<Object> Integer::clone() {
    return std::make_unique<Integer>(*this);
}

std::string Integer::describe() {
    return "[Integer]";
}

IntegerArray::IntegerArray(std::size_t length) : Object(INTEGER_ARRAY) {
    length_ = length;
    for (std::size_t i = 0; i < length; i++)
        values_[i] = 0;
}

IntegerArray::~IntegerArray() {
    std::size_t size = sizeof(IntegerArray) + sizeof(values_[0]) * (length_-1);
    track_dealloc(size);
}

void IntegerArray::operator delete(void *ptr) {
    free(ptr);
}

std::unique_ptr<Object> IntegerArray::clone() {
    auto arr = IntegerArray::create(length_);
    for (std::size_t i = 0; i < length_; i++)
        arr->values_[i] = values_[i];
    return arr;
}

std::string IntegerArray::describe() {
    return "[Integer(" + std::to_string(length_) + ")]";
}

std::int64_t &IntegerArray::at(std::int64_t idx) {
    if (idx < 0 || static_cast<std::size_t>(idx) >= length_)
        throw std::out_of_range("array index out of bounds");
    return values_[idx];
}

/* static */
std::unique_ptr<IntegerArray> IntegerArray::create(std::int64_t length) {
    if (length < 1 || length > 256)
        throw std::length_error("invalid array length");
    std::size_t size = sizeof(IntegerArray) + sizeof(values_[0]) * (length-1);
    track_alloc(size);
    auto mem = malloc(size);
    return std::unique_ptr<IntegerArray>(new (mem) IntegerArray(length));
}

String::String(const std::string &str) : Object(STRING) {
    if (str.size() > 1024)
        throw std::length_error("string too large");
    track_alloc(sizeof(String) + str.size());
    str_ = static_cast<char *>(malloc(str.size()));
    memcpy(str_, str.data(), str.size());
    length_ = str.size();
}

String::String(const String &other) : Object(STRING) {
    length_ = other.length_;
    track_alloc(sizeof(String) + length_);
    str_ = static_cast<char *>(malloc(length_));
    memcpy(str_, other.str_, length_);
}

String::~String() {
    track_dealloc(sizeof(String) + length_);
}

String &String::operator=(const String &other) {
    track_dealloc(length_);
    length_ = other.length_;
    free(str_);
    track_alloc(length_);
    str_ = static_cast<char *>(malloc(length_));
    memcpy(str_, other.str_, length_);
    return *this;
}

std::unique_ptr<Object> String::clone() {
    return std::make_unique<String>(*this);
}

std::string String::describe() {
    return "[String]";
}

std::string String::to_std() {
    return std::string(str_, length_);
}

} // namespace objects
} // namespace lang
