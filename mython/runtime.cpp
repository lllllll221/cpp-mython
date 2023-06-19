#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    if (object) {
        String *str = object.TryAs<String>();
        if (str) {
            return !str->GetValue().empty();
        }
        Number *num = object.TryAs<Number>();
        if (num) {
            return num->GetValue() != 0;
        }
        Bool *b = object.TryAs<Bool>();
        if (b) {
            return b->GetValue();
        }
    }
    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (class_.GetMethod("__str__")) {
        os << Call("__str__",  {}, context).TryAs<String>()->GetValue();
    }
    else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    if (const Method* meth = class_.GetMethod(method)) {
      if (meth->formal_params.size() == argument_count) {
        return true;
      }
    }
    return false;
}

Closure& ClassInstance::Fields() {
    return closures_;
}

const Closure& ClassInstance::Fields() const {
    return closures_;
}

ClassInstance::ClassInstance(const Class& cls)
    : class_(cls) {

    closures_["self"] = ObjectHolder::Share(*this);
}

ObjectHolder ClassInstance::Call(const std::string& method,
                                const std::vector<ObjectHolder>& actual_args,
                                Context& context) {

    if (HasMethod(method, actual_args.size())){
        auto method_ptr = class_.GetMethod(method);

        Closure args;

        for (size_t i = 0; i < actual_args.size(); ++i) {
            args[method_ptr->formal_params.at(i)] = actual_args[i];
        }
        args["self"] = ObjectHolder::Share(*this);

        return method_ptr->body->Execute(args, context);

    }

    throw std::runtime_error("method not found"s);
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
    : name_(name), methods_(std::move(methods)), parent_(parent) { }

const Method* Class::GetMethod(const std::string& name) const {
    for (auto& method : methods_) {
        if (method.name == name) {
            return &method;
        }
    }

    if (parent_ != nullptr) {
        return parent_->GetMethod(name);
    }

    return nullptr;
}

[[nodiscard]] const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
    os << "Class " << name_;
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.TryAs<Number>() && rhs.TryAs<Number>() ) {
        return lhs.TryAs<Number>()->GetValue() == rhs.TryAs<Number>()->GetValue();
    }
    else if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
        return lhs.TryAs<Bool>()->GetValue() == rhs.TryAs<Bool>()->GetValue();
    }
    else if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
        return lhs.TryAs<String>()->GetValue() == rhs.TryAs<String>()->GetValue();
    }
    else if (lhs.Get() == ObjectHolder::None().Get() && rhs.Get() == ObjectHolder::None().Get()) {
      return true;
    }
    else if (lhs.TryAs<ClassInstance>()) {
        if (lhs.TryAs<ClassInstance>()->HasMethod("__eq__",  1)) {
            return lhs.TryAs<ClassInstance>()->Call("__eq__",  {rhs}, context).TryAs<Bool>()->GetValue();
        }
    }
    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.TryAs<Number>() && rhs.TryAs<Number>() ) {
        return lhs.TryAs<Number>()->GetValue() < rhs.TryAs<Number>()->GetValue();
    }
    else if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
        return lhs.TryAs<Bool>()->GetValue() < rhs.TryAs<Bool>()->GetValue();
    }
    else if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
        return lhs.TryAs<String>()->GetValue() < rhs.TryAs<String>()->GetValue();
    }
    else if (lhs.TryAs<ClassInstance>()) {
        if (lhs.TryAs<ClassInstance>()->HasMethod("__lt__",  1)) {
            std::vector<ObjectHolder> args = {rhs};

            return lhs.TryAs<ClassInstance>()->Call("__lt__",  args, context).TryAs<Bool>()->GetValue();
        }
    }
    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs,  context);
    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    try {
        auto eq = Equal(lhs, rhs, context);
        auto lt = Less(lhs, rhs, context);
        return (!eq && !lt);
    }
    catch (...) {
        throw std::runtime_error("Cannot compare objects for equality"s);
    }
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    try {
        auto eq = Equal(lhs, rhs, context);
        auto lt = Less(lhs, rhs, context);
        return (eq || lt);
    }
    catch (...) {
        throw std::runtime_error("Cannot compare objects for equality"s);
    }
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime
