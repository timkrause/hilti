

#ifndef HILTI_EXPRESSION_H
#define HILTI_EXPRESSION_H

#include <ast/expression.h>

#include "coercer.h"
#include "common.h"
#include "constant-coercer.h"
#include "ctor.h"
#include "id.h"
#include "scope.h"
#include "variable.h"

namespace hilti {

/// Base class for expression nodes.
class Expression : public ast::Expression<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// l: An associated location.
    Expression(const Location& l = Location::None) : ast::Expression<AstInfo>(l)
    {
    }

    /// Returns a fully flattened list of all atomic sub-expressions.
    ///
    /// Must be overridden by derived classes. For atomic expressision, the
    /// returned list should contain just the epxression itself.
    virtual std::list<shared_ptr<hilti::Expression>> flatten() = 0;

    /// Returns true if this expression accesses a hoisted value.
    /// Can be overridden by derived classes. The default implementation returns always false.
    virtual bool hoisted();

    /// Returns a readable one-line representation of the expression.
    string render() override;

    ACCEPT_VISITOR_ROOT();
};

namespace expression {

/// AST node for a list expressions.
class List : public hilti::Expression, public ast::expression::mixin::List<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// exprs: A list of individual expressions.
    ///
    /// l: An associated location.
    List(const expression_list& exprs, const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::List<AstInfo>(this, exprs)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override;

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for a constructor expression.
class Ctor : public hilti::Expression, public ast::expression::mixin::Ctor<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// values: The list of values representing the arguments to the constructor.
    ///
    /// type: The type of the constructed value.
    ///
    /// l: An associated location.
    Ctor(shared_ptr<hilti::Ctor> ctor, const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::Ctor<AstInfo>(this, ctor)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override;

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for an expression referencing a constant
class Constant : public hilti::Expression, public ast::expression::mixin::Constant<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// constant: The constant.
    ///
    /// l: An associated location.
    Constant(shared_ptr<hilti::Constant> constant, const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::Constant<AstInfo>(this, constant)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override;

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for an expression referencing a variable.
class Variable : public hilti::Expression, public ast::expression::mixin::Variable<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// var: The variable.
    ///
    /// l: An associated location.
    Variable(shared_ptr<hilti::Variable> var, const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::Variable<AstInfo>(this, var)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override
    {
        return {this->sharedPtr<hilti::Expression>()};
    }

    bool hoisted() override;

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for an expression referencing a type.
class Type : public hilti::Expression, public ast::expression::mixin::Type<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// type: The type.
    ///
    /// l: An associated location.
    Type(shared_ptr<hilti::Type> type, const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::Type<AstInfo>(this, type)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override
    {
        return {this->sharedPtr<hilti::Expression>()};
    }

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for an expression constructing a type's default value.
class Default : public hilti::Expression, public ast::expression::mixin::Default<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// type: The type.
    ///
    /// l: An associated location.
    Default(shared_ptr<hilti::Type> type, const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::Default<AstInfo>(this, type)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override
    {
        return {this->sharedPtr<hilti::Expression>()};
    }

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for an expression referencing a statement block.
class Block : public hilti::Expression, public ast::expression::mixin::Block<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// block: The block.
    ///
    /// l: An associated location.
    Block(shared_ptr<hilti::statement::Block> block, const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::Block<AstInfo>(this, block)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override
    {
        return {this->sharedPtr<hilti::Expression>()};
    }

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for an expression referencing a module.
class Module : public hilti::Expression, public ast::expression::mixin::Module<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// module: The module.
    ///
    /// l: An associated location.
    Module(shared_ptr<hilti::Module> module, const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::Module<AstInfo>(this, module)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override
    {
        return {this->sharedPtr<hilti::Expression>()};
    }

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for an expression referencing a function.
class Function : public hilti::Expression, public ast::expression::mixin::Function<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// func: The function.
    ///
    /// l: An associated location.
    Function(shared_ptr<hilti::Function> func, const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::Function<AstInfo>(this, func)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override
    {
        return {this->sharedPtr<hilti::Expression>()};
    }

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for an expression referencing a function parameter.
class Parameter : public hilti::Expression, public ast::expression::mixin::Parameter<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// param: The parameter.
    ///
    /// l: An associated location.
    Parameter(shared_ptr<hilti::type::function::Parameter> param,
              const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::Parameter<AstInfo>(this, param)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override
    {
        return {this->sharedPtr<hilti::Expression>()};
    }

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for an expression referencing an ID.
class ID : public hilti::Expression, public ast::expression::mixin::ID<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// id: The ID.
    ///
    /// l: An associated location.
    ID(shared_ptr<hilti::ID> id, const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::ID<AstInfo>(this, id)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override
    {
        return {this->sharedPtr<hilti::Expression>()};
    }

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for an expression that represents another expression coerced to
/// a different type.
class Coerced : public hilti::Expression, public ast::expression::mixin::Coerced<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// expr: The original expression.
    ///
    /// dst: The type it is being coerced into. Note that the assumption is
    /// that the type coercion is legal.
    ///
    /// l: An associated location.
    Coerced(shared_ptr<hilti::Expression> expr, shared_ptr<hilti::Type> dst,
            const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::Coerced<AstInfo>(this, expr, dst)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override
    {
        return {this->sharedPtr<hilti::Expression>()};
    }

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for an expression that's used internal by the code generator.
class CodeGen : public hilti::Expression, public ast::expression::mixin::CodeGen<AstInfo> {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// type: The type of the expression.
    ///
    /// cookie: A value left to the interpretation of the code generator.
    ///
    /// l: An associated location.
    CodeGen(shared_ptr<hilti::Type> type, void* cookie, const Location& l = Location::None)
        : hilti::Expression(l), ast::expression::mixin::CodeGen<AstInfo>(this, type, cookie)
    {
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override
    {
        return {this->sharedPtr<hilti::Expression>()};
    }

    ACCEPT_VISITOR(hilti::Expression);
};

/// AST node for a void value to be returned by functions that don't have
/// anything to return. This is a place-holder that should never be further
/// used.
class Void : public hilti::Expression {
    AST_RTTI
public:
    /// Constructor.
    ///
    /// l: An associated location.
    Void(const Location& l = Location::None) : hilti::Expression(l)
    {
    }

    shared_ptr<Type> type() const override
    {
        return std::make_shared<type::Void>(location());
    }

    std::list<shared_ptr<hilti::Expression>> flatten() override
    {
        return {this->sharedPtr<hilti::Expression>()};
    }

    ACCEPT_VISITOR(hilti::Expression);
};
}
}

#endif
