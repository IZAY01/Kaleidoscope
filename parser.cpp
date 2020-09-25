static int CurTok;
static int getNextToken() {
    return CurTok = gettok();
}


static std::map<char, int> BinopPrecedence;


static int GetTokPrecedence() {
    if(!isascii(CurTok))
        return -1;

    int TokPrec = BinopPrecedence[CurTok];
    if(TokPrec <= 0)
        return -1;
    else
        return TokPrec;
}


std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}


std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}


static std::unique_ptr<ExprAST> ParseExpression();


static std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken();
    return std::move(Result);
}


static std::unique_ptr<ExprAST> ParseParenExpr() {
    getNextToken();
    auto V = ParseExpression();
    if(!V) 
        return nullptr;

    if(CurTok != ')')
        return LogError("expected ')'");
    else
        getNextToken();

    return V;
}


static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;

    getNextToken();

    if(CurTok != '(') 
        return std::make_unique<VariableExprAST>(IdName);

    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> Args;
    if(CurTok != ')') {
        while(true) {
            if(auto Arg = ParseExpression())
                Args.push_back(std::move(Arg));
            else
                return nullptr;

            if(CurTok == ')') 
                break;

            if(CurTok != ',')
                return LogError("Expected ')' or ',' in argument list");

            getNextToken();
        }
    }

    getNextToken();

    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}


static std::unique_ptr<ExprAST> ParseIfExpr() {
    getNextToken();

    auto Cond = ParseExpression();
    if(!Cond)
        return nullptr;

    if(CurTok != tok_then)
        return LogError("expected then");
    getNextToken();

    auto Then = ParseExpression();
    if(!Then)
        return nullptr;

    if(CurTok != tok_else)
        return LogError("expected else");
    getNextToken();

    auto Else = ParseExpression();
    if(!Else)
        return nullptr;

    return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
}


static std::unique_ptr<ExprAST> ParseForExpr() {
    getNextToken();

    if(CurTok != tok_indentifier)
        return LogError("expected identifier after for");
    std::string IdName = IdentifierStr;
    getNextToken();

    if(CurTok != '=')
        return LogError("expected '=' after for");
    getNextToken();


}
