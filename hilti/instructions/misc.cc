

iBegin(Misc::Select, "select")
    iTarget(optype::any);
    iOp1(optype::boolean, true);
    iOp2(optype::any, true);
    iOp3(optype::any, true);

    iValidate
    {
        canCoerceTo(op2, target->type());
        canCoerceTo(op3, target->type());
    }

    iDoc(R"(
        Returns *op2* if *op1* is True, and *op3* otherwise.
    )");
iEnd

iBegin(Misc::SelectValue, "select.value")
    iTarget(optype::any);
    iOp1(optype::any, true);
    iOp2(optype::tuple, true);
    iOp3(optype::optional(optype::any), true);

    iValidate
    {
        auto ty_op1 = op1->type();
        auto ty_op2 = ast::rtti::checkedCast<type::Tuple>(op2->type());

        if ( ! ast::type::hasTrait<type::trait::ValueType>(ty_op1) ) {
            error(ty_op1, "operand 1 must a value type");
            return;
        }

        if ( op3 )
            canCoerceTo(op3, target->type());

        for ( auto t : ty_op2->typeList() ) {
            auto tt = ast::rtti::tryCast<type::Tuple>(t);
            if ( ! tt || tt->typeList().size() != 2 ) {
                error(ty_op2, "operand 2 must a tuple of 2-tuples");
                return;
            }

            auto l = tt->typeList();
            auto i = l.begin();
            auto t1 = *i++;
            auto t2 = *i++;

            canCoerceTo(op1, t1);
            canCoerceTo(t2, target->type());
        }
    }

    iDoc(R"(
        Matches *op1* against the first elements of set of 2-tuples *op2*. If a
        match is found, returns the second element of the corresponding 2-tuple.
        If multiple matches are found, one is returned but it's undefined
        which one. If no match is found, returns *op3* if given, or throws a
        ValueError otherwise.
    )");
iEnd

iBegin(Misc::Nop, "nop")

    iValidate
    {
    }

    iDoc(R"(
        Evaluates into nothing.
    )");
iEnd
