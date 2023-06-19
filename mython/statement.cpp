#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;
using runtime::ClassInstance;
using runtime::String;
using runtime::Method;
using runtime::Number;
using runtime::Bool;
using runtime::Class;

namespace {
    const string ADD_METHOD = "__add__"s;
    const string INIT_METHOD = "__init__"s;
}  // namespace

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    closure[var_] = rv_->Execute(closure, context);
    return closure.at(var_);
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
    : var_(var)
    , rv_(std::move(rv))
{
}

VariableValue::VariableValue(const std::string& var_name)
    : vars_({var_name})
{
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids)
    : vars_(dotted_ids)
{
}

ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    if (closure.count(vars_[0]) == 0) {
        throw runtime_error("there is no variable: "s.append(vars_[0]));
    }
    ObjectHolder object = closure.at(vars_[0]);

    for (size_t i = 1; i < vars_.size(); ++i) {
        if (auto* tmp = object.TryAs<ClassInstance>()) {
            object = tmp->Fields()[vars_[i]];
        }
        else {
            throw runtime_error("object is not ClassInstance "s);
        }
    }

    return object;
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    auto value = unique_ptr<VariableValue>(new VariableValue(name));
    return make_unique<Print>(std::move(value));
}

Print::Print(unique_ptr<Statement> argument)
{
    args_.push_back(std::move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args)
    : args_(std::move(args))
{
}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    ostream& out(context.GetOutputStream());
    size_t args_count = args_.size();
    for (size_t i = 0; i < args_count; ++i) {
        if (ObjectHolder object = args_[i]->Execute(closure, context)) {
            object->Print(out, context);
        }
        else {
            out << "None"s;
        }
        if (i != args_count-1) {
            out << " ";
        }
    }
    out << "\n";
    return ObjectHolder::None();
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                      std::vector<std::unique_ptr<Statement>> args)
    : object_(std::move(object))
    , method_(method)
    , args_(std::move(args))
{
}

ObjectHolder MethodCall::Execute(Closure& closur, Context& context) {
    ClassInstance* object = object_->Execute(closur, context).TryAs<ClassInstance>();
    std::vector<ObjectHolder> actual_args;
    for (const auto & arg : args_) {
        actual_args.push_back(arg->Execute(closur, context));
    }
    return object->Call(method_, actual_args, context);
}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    ObjectHolder object = argument_->Execute(closure, context);
    if (!object) {
        return ObjectHolder::Own(String("None"s));
    }
    ostringstream out;
    ClassInstance* inst_obj = object.TryAs<ClassInstance>();
    if (inst_obj && inst_obj->HasMethod("__str__", 0)) {
        inst_obj->Call("__str__", {}, context)->Print(out, context);
    }
    else {
        object->Print(out, context);
    }
    return ObjectHolder::Own(String(out.str()));
}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_object = lhs_->Execute(closure, context);
    ObjectHolder rhs_object = rhs_->Execute(closure, context);

    {
        Number* lhs_num = lhs_object.TryAs<Number>();
        Number* rhs_num = rhs_object.TryAs<Number>();
        if (lhs_num && rhs_num ) {
            return ObjectHolder::Own(Number(lhs_num->GetValue() + rhs_num->GetValue()));
        }
    }

    {
        String* lhs_str = lhs_object.TryAs<String>();
        String* rhs_str = rhs_object.TryAs<String>();
        if (lhs_str && rhs_str) {
            return ObjectHolder::Own(String(lhs_str->GetValue() + rhs_str->GetValue()));
        }
    }

    if (ClassInstance* lhs_inst = lhs_object.TryAs<ClassInstance>()) {
        if(lhs_inst->HasMethod(ADD_METHOD, 1)) {
            return lhs_inst->Call(ADD_METHOD, {rhs_object}, context);
        }
    }

    throw runtime_error("invalid add operation"s);
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_object = lhs_->Execute(closure, context);
    ObjectHolder rhs_object = rhs_->Execute(closure, context);

    {
        Number* lhs_num = lhs_object.TryAs<Number>();
        Number* rhs_num = rhs_object.TryAs<Number>();
        if (lhs_num && rhs_num ) {
            return ObjectHolder::Own(Number(lhs_num->GetValue() - rhs_num->GetValue()));
        }
    }

    throw runtime_error("invalid sub operation"s);
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_object = lhs_->Execute(closure, context);
    ObjectHolder rhs_object = rhs_->Execute(closure, context);

    {
        Number* lhs_num = lhs_object.TryAs<Number>();
        Number* rhs_num = rhs_object.TryAs<Number>();
        if (lhs_num && rhs_num ) {
            return ObjectHolder::Own(Number(lhs_num->GetValue() * rhs_num->GetValue()));
        }
    }

    throw runtime_error("invalid mult operation"s);
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_object = lhs_->Execute(closure, context);
    ObjectHolder rhs_object = rhs_->Execute(closure, context);

    {
        Number* lhs_num = lhs_object.TryAs<Number>();
        Number* rhs_num = rhs_object.TryAs<Number>();
        if (lhs_num && rhs_num ) {
            if (rhs_num->GetValue() == 0) {
                throw runtime_error("division by zero"s);
            }
            return ObjectHolder::Own(Number(lhs_num->GetValue() / rhs_num->GetValue()));
        }
    }

    throw runtime_error("invalid div operation"s);
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for (const auto& stmt : stmts_) {
        stmt->Execute(closure, context);
        if(closure.count("return_val") > 0) {
            return ObjectHolder::None();
        }
    }
    return ObjectHolder::None();
}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    closure["return_val"] = statement_->Execute(closure, context);
    return ObjectHolder::None();
}

ClassDefinition::ClassDefinition(ObjectHolder cls)
    : cls_(cls)
{
}

ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]]  Context& context) {
    std::string cls_name = cls_.TryAs<Class>()->GetName();
    closure[cls_name] = cls_;
    return ObjectHolder::None();
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                  std::unique_ptr<Statement> rv)
    : object_(object)
    , field_name_(field_name)
    , rv_(std::move(rv))
{
}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    Closure& fields = object_.Execute(closure, context).TryAs<runtime::ClassInstance>()->Fields();
    fields[field_name_] = rv_->Execute(closure, context);
    return fields.at(field_name_);
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
              std::unique_ptr<Statement> else_body)
    : condition_(std::move(condition))
    , if_body_(std::move(if_body))
    , else_body_(std::move(else_body))
{
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    bool selection = IsTrue(condition_->Execute(closure, context));
    if (selection) {
        if_body_->Execute(closure, context);
    }
    else if (else_body_) {
        else_body_->Execute(closure, context);
    }
    return ObjectHolder::None();
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    bool lhs_bool = IsTrue(lhs_->Execute(closure, context));
    if (!lhs_bool) {
        return ObjectHolder::Own(Bool(IsTrue(rhs_->Execute(closure, context))));
    }
    return ObjectHolder::Own(Bool(true));
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    bool lhs_bool = IsTrue(lhs_->Execute(closure, context));
    if (lhs_bool) {
        return ObjectHolder::Own(Bool(IsTrue(rhs_->Execute(closure, context))));
    }
    return ObjectHolder::Own(Bool(false));
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    return ObjectHolder::Own(Bool(!IsTrue(argument_->Execute(closure, context))));
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs))
    , cmp_(cmp)
{
}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    return ObjectHolder::Own(Bool(cmp_(lhs_->Execute(closure, context), rhs_->Execute(closure, context), context)));
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
    : class_(class_)
    , args_(std::move(args))
{
}

NewInstance::NewInstance(const runtime::Class& class_)
    : class_(class_)
{
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    ObjectHolder object = ObjectHolder::Own(ClassInstance(class_));
    const Method* method = class_.GetMethod(INIT_METHOD);
    if (!method || method->formal_params.size() != args_.size()) {
      return object;
    }
    std::vector<ObjectHolder> actual_args;
    for (const auto& arg : args_) {
        actual_args.push_back(arg->Execute(closure, context));
    }
    ObjectHolder init_object = object.TryAs<ClassInstance>()->Call(INIT_METHOD, actual_args, context);

    return init_object ? init_object : object;
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
    : body_(std::move(body))
{
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    body_->Execute(closure, context);
    if (closure.count("return_val") > 0) {
        return closure.at("return_val");
    }
    return ObjectHolder::None();
}

}  // namespace ast
