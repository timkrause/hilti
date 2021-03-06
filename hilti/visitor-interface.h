
#ifndef HILTI_VISITOR_INTERFACE_H
#define HILTI_VISITOR_INTERFACE_H

#include "common.h"

namespace hilti {

/// Visitor interface. This class defines one visit method for each type of
/// HILTI AST node that we have. Visitor implementation then override the
/// ones they need.
class VisitorInterface {
public:
    virtual ~VisitorInterface();

// This is autogenerated and has the visits() for all the
// classes defined in the file "nodes.decl".
#include <hilti/autogen/visitor-interface.h>

// This is autogenerated and has the visits() for all the (likewise
// generated) expression::operator_::* classes.
#include <hilti/autogen/instructions-visits.h>

protected:
    /// Internal function. This carries out the visitor callback.
    virtual void callAccept(shared_ptr<ast::NodeBase> node);
};
}

#endif
