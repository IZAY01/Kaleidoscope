static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::map<std::string, AllocaInst *> NamedValues;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;


Value *LogErrorV(const char *Str) {
    LogError(Str);
    return nullptr;
}


Function *getFunction(std::string Name) {
    if(auto *F = TheModule->getFunction(Name))
        return F;
    
    auto FI = FunctionProtos.find(Name);
    if(FI != FunctionProtos.end())
        return FI->second->codegen();
    else
        return nullptr;
}


static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction, StringRef VarName) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(Type::getDoubleTy(TheContext), nullptr, VarName);
}


Value *NumberExprAST::codegen() {
    return ConstantFP::get(TheContext, APFloat(Val));
}


Value *VariableExprAST::codegen() {
    Value *V = NamedValues[Name];
    if(!V)
        return LogErrorV("Unknown variable name");

    return Builder.CreateLoad(V, Name.c_str());
}


Value *UnaryExprAST::codegen() {
    Value *OperandV = Operand->codegen();
    if(!OperandV)
        return nullptr;

    Function *F = getFunction(std::string("unary") + Opcode);
    if(!F)
        return LogErrorV("Unknown unary operator");

    return Builder.CreateCall(F, OperandV, "unop");
}


Value *BinaryExprAST::codegen() {

}
