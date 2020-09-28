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
    if(Op == '=') {
        VariableExprAST *LHSE = static_cast<VariableExprAST *>(LHS.get());
        if(!LHSE)
            return LogErrorV("destination of '=' must be a variable");

        Value *Val = RHS->codegen();
        if(!Val)
            return nullptr;

        Value *VariableExprAST = NamedValues[LHSE->getName()];
        if(!Variable)
            return LogErrorV("Unknown variable name");

        Builder.CreateStore(Val, Variable);
        return Val;
    }

    Value *L = LHS->codegen();
    Value *R = RHS->codegen();
    if(!L || !R)
        return nullptr;

    switch(Op) {
        case '+':
            return Builder.CreateFAdd(L, R, "addtmp");
        case '-':
            return Builder.CreateFSub(L, R, "subtmp");
        case '*':
            return Builder.CreateFMul(L, R, "multmp");
        case '<':
            L = Builder.CreateFCmpULT(L, R, "cmptmp");
            return Builder.CreateUIToFP(L, Type::getDoubleTy(TheContext), "booltmp");
        default:
            break;
    }

    Function *F = getFunction(std::string("binary") + Op);
    assert(F && "binary operator not found!");

    Value *Ops[] = {L, R};
    return Builder.CreateCall(F, Ops, "binop")
}


Value *CallExprAST::codegen() {
    Function *CalleeF = getFunction(Callee);
    if (!CalleeF)
        return LogErrorV("Unknown function referenced");

    if(CalleeF->arg_size() != Args.size())
        return LogErrorV("Incorrect # arguments passed");

    std::vector<Value *> ArgsV;
    for(unsigned i = 0, e = Args.size(); i != e; ++i) {
        ArgsV.push_back(Args[i]->codegen());
        if(!ArgsV.back())
            return nullptr;
    }

    return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}


Value *IfExprAST::codegen() {
    Value *CondV = Cond->codegen();
    if(!CondV)
        return nullptr;

    CondV = Builder.CreateFCmpONE(CondV, ConstantFP::get(TheContext, APFloat(0.0)), "ifcond");
    Function *TheFunction = Builder.GetInsertBlock()->getParent();

    BasicBlock *ThenBB = BasicBlock::Create(TheContext, "then", TheFunction);
    BasicBlock *ElseBB = BasicBlock::Create(TheContext, "else");
    BasicBlock *MergeBB = BasicBlock::Create(TheContext, "ifcont");
    
    Builder.CreateCondBr(CondV, ThenBB, ElseBB);

    Builder.SetInsertPoint(ThenBB);

    Value *ThenV = Then->codegen();
    if(!ThenV)
        return nullptr;

    Builder.CreateBr(MergeBB);
    ThenBB = Builder.GetInsertBlock();

    TheFunction->getBasicBlockList().push_back(ElseBB);
    Builder.SetInsertPoint(ElseBB);

    Value *ElseV = Else->codegen();
    if (!ElseV)
    return nullptr;

    Builder.CreateBr(MergeBB);
    ElseBB = Builder.GetInsertBlock();

    TheFunction->getBasicBlockList().push_back(MergeBB);
    Builder.SetInsertPoint(MergeBB);
    PHINode *PN = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
}


Value *ForExprAST::codegen() {
      Function *TheFunction = Builder.GetInsertBlock()->getParent();

      AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

      Value *StartVal = Start->codegen();
      if (!StartVal)
          return nullptr;

      Builder.CreateStore(StartVal, Alloca);

      BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);

      Builder.CreateBr(LoopBB);

      Builder.SetInsertPoint(LoopBB);

      AllocaInst *OldVal = NamedValues[VarName];
      NamedValues[VarName] = Alloca;

      if (!Body->codegen())
          return nullptr;

      Value *StepVal = nullptr;
      if (Step) {
          StepVal = Step->codegen();
          if (!StepVal)
              return nullptr;
      } else {
          StepVal = ConstantFP::get(TheContext, APFloat(1.0));
      }

      Value *EndCond = End->codegen();
      if (!EndCond)
          return nullptr;

      Value *CurVar = Builder.CreateLoad(Alloca, VarName.c_str());
      Value *NextVar = Builder.CreateFAdd(CurVar, StepVal, "nextvar");
      Builder.CreateStore(NextVar, Alloca);

      EndCond = Builder.CreateFCmpONE(EndCond, ConstantFP::get(TheContext, APFloat(0.0)), "loopcond");

      BasicBlock *AfterBB = BasicBlock::Create(TheContext, "afterloop", TheFunction);

      Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

      Builder.SetInsertPoint(AfterBB);

      if (OldVal)
          NamedValues[VarName] = OldVal;
      else
          NamedValues.erase(VarName);

      return Constant::getNullValue(Type::getDoubleTy(TheContext));
}


Value *VarExprAST::codegen() {
    std::vector<AllocaInst *> OldBindings;

    Function *TheFunction = Builder.GetInsertBlock()->getParent();

    for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
        const std::string &VarName = VarNames[i].first;
        ExprAST *Init = VarNames[i].second.get();

        Value *InitVal;
        if (Init) {
            InitVal = Init->codegen();
            if (!InitVal)
                return nullptr;
        } else { 
            InitVal = ConstantFP::get(TheContext, APFloat(0.0));
        }

        AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
        Builder.CreateStore(InitVal, Alloca);

        OldBindings.push_back(NamedValues[VarName]);

        NamedValues[VarName] = Alloca;
    }

    Value *BodyVal = Body->codegen();
    if (!BodyVal)
        return nullptr;

    for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
        NamedValues[VarNames[i].first] = OldBindings[i];

    return BodyVal;
}


Function *PrototypeAST::codegen() {
    std::vector<Value *> Doubles(Args.size(), Type::getDoubleTy(TheContext));
    FunctionType *FT = FunctionType::get(Type::getDoubleTy(TheContext), Doubles, false);

    Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

    unsigned Idx = 0;
    for(auto &Arg : F->args())
        Arg.setName(Args[Idx++]);

    return F;
}


Function *FunctionAST::codegen() {
    auto &P = *Proto;
    FunctionProtos[Proto->getName()] = std::move(Proto);
    Function *TheFunction = getFunction(P.getName());
    if(!TheFunction)
        return nullptr;

    if(P.isBinaryOp())
        BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();

    BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
    Builder.SetInsertPoint(BB);

    NamedValues.clear();
    for(auto &Arg : TheFunction->args()) {
        AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());
        Builder.CreateStore(&Arg, Alloca);
        NamedValues[std::string(Arg.getName())] = Alloca;
    }

    if(Value *RetVal = Body->codegen()) {
        Builder.CreateRet(RetVal);
        verifyFunction(*TheFunction);
        return TheFunction;
    }

    TheFunction->eraseFromParent();

    if(P.isBinaryOp())
        BinopPrecedence.erase(P.getOperatorName());

    return nullptr;
}
