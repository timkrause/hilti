
#include "linker.h"
#include "util.h"
#include "codegen.h"
#include "../options.h"
#include "../context.h"

// TODO: THis needs cleanup.

using namespace hilti;
using namespace codegen;

// LLVM pass that replaces all calls to @hlt.globals.base.<Module> with code
// computing base address for the module's globals.
class GlobalsBasePass : public llvm::BasicBlockPass
{
public:
   GlobalsBasePass() : llvm::BasicBlockPass(ID) {}

   // Maps @hlt.globals.base.<Module> functions to the base index in the joint globals struct.
   typedef std::map<llvm::Value *, llvm::Value *> transform_map;
   transform_map tmap;
   llvm::Type* globals_type;
   llvm::Type* execution_context_type;

   // Records which function we have already instrumented with code to
   // calculate the start of our globals, inluding the value to reference it.
   typedef std::map<llvm::Function*, llvm::Value *> function_set;
   function_set functions;

   bool runOnBasicBlock(llvm::BasicBlock &bb) override;

   const char* getPassName() const override { return "hilti::codegen::GlobalsBasePass"; }

   static char ID;

};

char GlobalsBasePass::ID = 0;

bool GlobalsBasePass::runOnBasicBlock(llvm::BasicBlock &bb)
{
    llvm::LLVMContext& ctx = llvm::getGlobalContext();

    // We can't replace the instruction while we're iterating over the block
    // so first identify and then replace afterwards.
    std::list<std::pair<llvm::CallInst *, llvm::Value *>> replace;

    for ( auto i = bb.begin(); i != bb.end(); ++i ) {
        auto call = llvm::dyn_cast<llvm::CallInst>(i);
        if ( ! call )
            continue;

        auto j = tmap.find(call->getCalledFunction());

        if ( j != tmap.end() )
            replace.push_back(std::make_pair(call, j->second));
    }

    if ( ! replace.size() )
        return false;

    llvm::Value* base_addr = nullptr;
    llvm::Function* func = bb.getParent();

    auto f = functions.find(func);

    if ( f != functions.end() )
        base_addr = f->second;

    else {
        // Get the execution context argument.
        llvm::Value* ectx = nullptr;

        for ( auto a = func->arg_begin(); a != func->arg_end(); ++a )
            if ( a->getName() == symbols::ArgExecutionContext )
                ectx = a;

        if ( ! ectx )
            throw InternalError("function accessing global does not have an execution context parameter");

        // Add instructions at the beginning of the function that give us the
        // address of the global variables inside the execution context.
        auto zero = llvm::ConstantInt::get(llvm::Type::getIntNTy(ctx, 32), 0);
        auto globals_idx = llvm::ConstantInt::get(llvm::Type::getIntNTy(ctx, 32), hlt::Globals);
        std::vector<llvm::Value*> indices;
        indices.push_back(zero);
        indices.push_back(globals_idx);

        auto i0 = llvm::CastInst::CreatePointerCast(ectx, execution_context_type);
        auto i1 = llvm::GetElementPtrInst::Create(i0, indices, "");
        auto i2 = llvm::CastInst::CreatePointerCast(i1, llvm::PointerType::get(globals_type, 0));

        llvm::BasicBlock& entry_block = func->getEntryBlock();
        llvm::BasicBlock::InstListType& instrs = entry_block.getInstList();

        instrs.push_front(i2);
        instrs.push_front(i1);
        instrs.push_front(i0);

        functions.insert(std::make_pair(func, i2));
        base_addr = i2;
    }

    // Now replace the fake calls with the actual address calculation.
    for ( auto i : replace ) {
        auto zero = llvm::ConstantInt::get(llvm::Type::getIntNTy(ctx, 32), 0);

        std::vector<llvm::Value*> indices;
        indices.push_back(zero);
        indices.push_back(i.second);
        auto gep = llvm::GetElementPtrInst::Create(base_addr, indices, "");

        llvm::ReplaceInstWithInst(i.first, gep);
    }

    return true;
}

CompilerContext* Linker::context() const
{
    return _ctx;
}

const Options& Linker::options() const
{
    return _ctx->options();
}

void Linker::fatalError(const string& msg, const string& file, const string& error)
{
    string err = error.size() ? ::util::fmt(" (%s)", error) : "";

    if ( file.empty() )
        ast::Logger::fatalError(::util::fmt("error linking: %s", msg));
    else
        ast::Logger::fatalError(::util::fmt("error linking %s: %s%s", file, msg, err));
}

llvm::Module* Linker::link(string output, const std::list<llvm::Module*>& modules)
{
    if ( options().cgDebugging("linker") )
        debugSetLevel(1);

    if ( modules.size() == 0 )
        fatalError("no modules given to linker");

    llvm::Module* composite = new llvm::Module(output, llvm::getGlobalContext());
    llvm::Linker linker(composite);

    std::list<string> module_names;

    // Link in all the modules.

    for ( auto i : modules ) {

        auto name = CodeGen::llvmGetModuleIdentifier(i);

        if ( isHiltiModule(i) )
            module_names.push_back(name);

        linkInModule(&linker, i);

        if ( options().verify && ! util::llvmVerifyModule(linker.getModule()) )
            fatalError("verification failed", name);
    }

    // Link in bitcode libraries.

    for ( auto i : _bcs ) {
        string err;

        llvm::OwningPtr<llvm::MemoryBuffer> buffer;

        if ( llvm::MemoryBuffer::getFile(i.c_str(), buffer) )
            fatalError("reading bitcode failed", i);

        llvm::Module* bc = llvm::ParseBitcodeFile(buffer.get(), llvm::getGlobalContext(), &err);

        if ( ! bc )
            fatalError("parsing bitcode failed", i, err);

        linkInModule(&linker, bc);
    }

    // Link native library.

    for ( auto i : _natives )
        linkInNativeLibrary(&linker, i);

    if ( module_names.size() ) {
        // In LLVM <= 3.3, we need to add these to a separate module that we
        // then link in; if we added it to the module directly, names
        // wouldn't be unified correctly. Starting with LLVM 3.4, we can add
        // it to the composite module directly.
#ifdef HAVE_LLVM_33
        auto target_module = new ::llvm::Module("__linker_stuff", llvm::getGlobalContext());
#else
        auto target_module = linker.getModule();
#endif
        addModuleInfo(target_module, module_names, linker.getModule());
        addGlobalsInfo(target_module, module_names, linker.getModule());

#ifdef HAVE_LLVM_33
        linkInModule(&linker, target_module);
#endif

        makeHooks(module_names, linker.getModule());
    }

    return linker.getModule();
}

bool Linker::isHiltiModule(llvm::Module* module)
{
    string id = CodeGen::llvmGetModuleIdentifier(module);
    string name = string(symbols::MetaModule) + "." + id;

    auto md = module->getNamedMetadata(name);
    bool is_hilti = (md != nullptr);

    if ( is_hilti )
        debug(1, ::util::fmt("module %s has HILTI meta data", id.c_str()));
    else
        debug(1, ::util::fmt("module %s does not have HILTI meta data", id.c_str()));

    return is_hilti;
}

llvm::NamedMDNode* Linker::moduleMeta(llvm::Module* module, shared_ptr<ID> id)
{
    string name = string(symbols::MetaModule) + "." + id->name();
    auto md = module->getNamedMetadata(name);
    assert(md);
    return md;
}

void Linker::joinFunctions(llvm::Module* dst, const char* new_func, const char* meta, llvm::FunctionType* default_ftype, const std::list<string>& module_names, llvm::Module* module)
{
    auto md = module->getNamedMetadata(meta);

    IRBuilder* builder = nullptr;
    llvm::LLVMContext& ctx = llvm::getGlobalContext();
    llvm::Function* nfunc = nullptr;

    if ( ! md )
        return;

#if 0
    if ( ! md ) {
        // No such meta node, i.e., no module defines the function
        //
        // Create a function with the given type so that it exists, even if
        // empty. In this case, the function type doesn't need to have the
        // exact types as long as the cc does the right thing.
        nfunc = llvm::Function::Create(default_ftype, llvm::Function::ExternalLinkage, new_func, dst);
        nfunc->setCallingConv(llvm::CallingConv::C);
        builder = util::newBuilder(ctx, llvm::BasicBlock::Create(ctx, "", nfunc));
        builder->CreateRetVoid();
        return;
    }
#endif

    for ( int i = 0; i < md->getNumOperands(); ++i ) {
        auto node = llvm::cast<llvm::MDNode>(md->getOperand(i));
        auto func = llvm::cast<llvm::Function>(node->getOperand(0));

        if ( ! builder ) {
            // Create new function with same signature.
            auto ftype_ptr = llvm::cast<llvm::PointerType>(func->getType());
            auto old_ftype = llvm::cast<llvm::FunctionType>(ftype_ptr->getElementType());
            std::vector<llvm::Type*> params(old_ftype->param_begin(), old_ftype->param_end());
            auto ftype = llvm::FunctionType::get(old_ftype->getReturnType(), params, false);
            nfunc = llvm::Function::Create(ftype, llvm::Function::ExternalLinkage, new_func, dst);
            nfunc->setCallingConv(llvm::CallingConv::C);
            builder = util::newBuilder(ctx, llvm::BasicBlock::Create(ctx, "", nfunc));
        }

        std::vector<llvm::Value*> params;
        int n = 1;
        for ( auto a = nfunc->arg_begin(); a != nfunc->arg_end(); ++a ) {
            a->setName(::util::fmt("__arg%d", n++));
            params.push_back(a);
        }

        util::checkedCreateCall(builder, "Linker", func, params);
    }

    builder->CreateRetVoid();
}

struct HookImpl {
    llvm::Value* func;
    int64_t priority;
    int64_t group;

    // Inverse sort.
    bool operator<(const HookImpl& other) const { return priority > other.priority; }
};

struct HookDecl {
    llvm::Value* func;
    llvm::Type* result;
    std::vector<HookImpl> impls;
};

static void _debugDumpHooks(Linker* linker, const std::map<string, HookDecl>& hooks)
{
    for ( auto h : hooks ) {
        linker->debug(1, ::util::fmt("hook %s - Trigger function '%s'", h.first.c_str(), h.second.func->getName().str().c_str()));
        for ( auto i : h.second.impls )
            linker->debug(1, ::util::fmt("   -> '%s' (priority %" PRId64 ", group %" PRId64 ")",
                                         i.func->getName().str().c_str(), i.priority, i.group));
    }
}

void Linker::makeHooks(const std::list<string>& module_names, llvm::Module* module)
{
    auto decls = module->getNamedMetadata(symbols::MetaHookDecls);
    auto impls = module->getNamedMetadata(symbols::MetaHookImpls);

    if ( ! decls ) {
        debug(1, "no hooks declared in any module");
        return;
    }

    std::map<string, HookDecl> hooks;

    // Collects all the declarations first.
    for ( int i = 0; i < decls->getNumOperands(); ++i ) {
        HookDecl decl;

        auto node = llvm::cast<llvm::MDNode>(decls->getOperand(i));
        auto name = llvm::cast<llvm::MDString>(node->getOperand(0))->getString();

        auto op1 = llvm::cast_or_null<llvm::Constant>(node->getOperand(1));

        if ( ! op1 || op1->isNullValue() )
            // Optimization may throw out the function if it's not
            // used/empty, in which case the pointer in the meta-data will be
            // set to null.
            continue;

        decl.func = llvm::cast<llvm::Function>(op1);

        if ( node->getNumOperands() > 2 )
            decl.result = node->getOperand(2)->getType();
        else
            decl.result = nullptr;

        auto h = hooks.find(name);

        if ( h == hooks.end() )
            hooks.insert(std::make_pair(name, decl));

        else {
            // Already declared, make sure it matches.
            auto other = (*h).second;

            //if ( decl.func != other.func || decl.result != other.result )
            //    fatalError(::util::fmt("inconsistent declarations found for hook %s", name.str().c_str()));
        }
    }

    if ( impls ) {

        // Now add the implementations.
        for ( int i = 0; i < impls->getNumOperands(); ++i ) {
            auto node = llvm::cast<llvm::MDNode>(impls->getOperand(i));
            auto name = llvm::cast<llvm::MDString>(node->getOperand(0))->getString();

            auto h = hooks.find(name);

            if ( h == hooks.end() )
                // Hook isn't declared, which means it will never be called.
                continue;

            HookImpl impl;
            impl.func = llvm::cast<llvm::Function>(node->getOperand(1));
            impl.priority = llvm::cast<llvm::ConstantInt>(node->getOperand(2))->getSExtValue();
            impl.group = llvm::cast<llvm::ConstantInt>(node->getOperand(3))->getSExtValue();

            (*h).second.impls.push_back(impl);
        }

    }

    // Reverse sort the implementation by priority.
    for ( auto h = hooks.begin(); h != hooks.end(); ++h ) {
        std::sort((*h).second.impls.begin(), (*h).second.impls.end());
    }

    _debugDumpHooks(this, hooks);

    // Finally, we can build the functions that call the hook implementations.

    llvm::LLVMContext& ctx = llvm::getGlobalContext();
    auto true_ = llvm::ConstantInt::get(llvm::Type::getIntNTy(ctx, 1), 1);
    auto false_ = llvm::ConstantInt::get(llvm::Type::getIntNTy(ctx, 1), 0);

    for ( auto h : hooks ) {
        auto decl = h.second;

        // Add a body to the entry function. The body calls each function
        // successively as long as none return true as an indication that it
        // wants to stop hook execution.
        auto func = llvm::cast<llvm::Function>(decl.func);
        auto stopped = util::newBuilder(ctx, llvm::BasicBlock::Create(ctx, "stopped"));
        auto next = util::newBuilder(ctx, llvm::BasicBlock::Create(ctx, "hook", func));

        for ( auto i : decl.impls ) {
            IRBuilder* current = next;
            next = util::newBuilder(ctx, llvm::BasicBlock::Create(ctx, "hook", func));

            std::vector<llvm::Value *> args;

            for ( auto p = func->arg_begin(); p != func->arg_end(); ++p )
                args.push_back(p);

            auto result = util::checkedCreateCall(current, "Linker::makeHooks", i.func, args);

            llvm::ConstantInt::get(llvm::Type::getIntNTy(ctx, 1), 0);
            current->CreateCondBr(result, stopped->GetInsertBlock(), next->GetInsertBlock());
        }

        next->CreateRet(false_);
        stopped->CreateRet(true_);
        func->getBasicBlockList().push_back(stopped->GetInsertBlock());
    }
}

void Linker::addModuleInfo(llvm::Module* dst, const std::list<string>& module_names, llvm::Module* module)
{
    llvm::LLVMContext& ctx = llvm::getGlobalContext();

    auto voidp = llvm::PointerType::get(llvm::IntegerType::get(ctx, 8), 0);
    std::vector<llvm::Type*> args = { voidp };
    auto ftype = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), args, false);

    joinFunctions(dst, symbols::FunctionModulesInit, symbols::MetaModuleInit, ftype, module_names, module);
}

void Linker::addGlobalsInfo(llvm::Module* dst, const std::list<string>& module_names, llvm::Module* module)
{
    llvm::LLVMContext& ctx = llvm::getGlobalContext();

    auto voidp = llvm::PointerType::get(llvm::IntegerType::get(ctx, 8), 0);
    std::vector<llvm::Type*> args = { voidp };
    auto ftype = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), args, false);

    joinFunctions(dst, symbols::FunctionGlobalsInit, symbols::MetaGlobalsInit, ftype, module_names, module);
    joinFunctions(dst, symbols::FunctionGlobalsDtor, symbols::MetaGlobalsDtor, ftype, module_names, module);

    // Create the joint globals type and determine its size.

    std::vector<llvm::Type*> globals;
    GlobalsBasePass::transform_map tmap;

    for ( auto name : module_names ) {
        // Add module's globals to the joint type and keep track of the index in there.
        auto type = module->getTypeByName(symbols::TypeGlobals + string(".") + name);
        auto base_func = module->getFunction(symbols::FuncGlobalsBase + string(".") + name);

        if ( ! type )
            continue;

        if ( ! base_func ) {
            debug(1, ::util::fmt("no globals.base function defined for module %s", name.c_str()));
            continue;
        }

        if ( type && base_func ) {
            auto idx = llvm::ConstantInt::get(llvm::Type::getIntNTy(ctx, 32), globals.size());
            tmap.insert(std::make_pair(base_func, idx));
            globals.push_back(type);
        }
    }

    // Create the joint globals type.
    auto ty_globals = llvm::StructType::create(ctx, globals, symbols::TypeGlobals);

    // Create the function returning the size of the globals struct, using the "portable sizeof" idiom ...
    auto null = llvm::Constant::getNullValue(llvm::PointerType::get(ty_globals, 0));
    auto one = llvm::ConstantInt::get(llvm::Type::getIntNTy(ctx, 32), 1);
    auto addr = llvm::ConstantExpr::getGetElementPtr(null, one);
    auto size = llvm::ConstantExpr::getPtrToInt(addr, llvm::Type::getIntNTy(ctx, 64));

    auto gftype = llvm::FunctionType::get(llvm::Type::getIntNTy(ctx, 64), false);
    auto gfunc = llvm::Function::Create(gftype, llvm::Function::ExternalLinkage, symbols::FunctionGlobalsSize, dst);
    gfunc->setCallingConv(llvm::CallingConv::C);
    auto builder = util::newBuilder(ctx, llvm::BasicBlock::Create(ctx, "", gfunc));
    builder->CreateRet(size);

    // Now run a pass over all the modules replacing the
    // @hlt.globals.base.<Module>() calls.
    auto pass = new GlobalsBasePass();
    pass->tmap = tmap;
    pass->globals_type = ty_globals;

    auto md = Linker::moduleMeta(module, std::make_shared<hilti::ID>(module_names.front()));
    assert(md);
    pass->execution_context_type = md->getOperand(symbols::MetaModuleExecutionContextType)->getOperand(0)->getType();

    llvm::PassManager mgr;
    mgr.add(pass);
    mgr.run(*module);
}

void hilti::codegen::Linker::linkInModule(llvm::Linker* linker, llvm::Module* module)
{
    auto name = CodeGen::llvmGetModuleIdentifier(module);

    debug(1, ::util::fmt("linking in module %s", name));

    string err;

    if ( linker->linkInModule(module, &err) )
        fatalError("module linking error", name, err);
}

void hilti::codegen::Linker::linkInNativeLibrary(llvm::Linker* linker, const string& library)
{
    fatalError("native libraries not supported currently", library);
}

