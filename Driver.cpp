static void InitializeModuleAndPassManager() {
    TheModule = std::make_unique<Module>("my cool jit", TheContext);
}


static void HandleDefinition() {
    if(auto FnAST = ParseDefinition()) {
        if(auto *FnIR = FnAST->codegen()) {
            fprintf(stderr, "Read function definition:");
            FnIR->print(errs());
            fprintf(stderr, "\n");
        }
    } else {
        getNextToken();
    }
}


static void HandleTopLevelExpression() {
    if(auto FnAST = ParseTopLevelExpr()) {
        FnAST->codegen();
    } else {
        getNextToken();
    }
}


static void MainLoop() {
    while(true) {
        switch(CurTok) {
            case tok_eof:
                return;
            case ';':
                getNextToken();
                break;
            case tok_def:
                HandleDefinition();
                break;
            case tok_extern:
                HandleExtern();
                break;
            default:
                HandleTopLevelExpression();
                break;
        }
    }
}
