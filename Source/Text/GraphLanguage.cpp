#include "GraphLanguage.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace duodsp::text
{
namespace
{
enum class TokenType
{
    identifier,
    number,
    atControl,
    lParen,
    rParen,
    comma,
    semicolon,
    assign,
    plus,
    minus,
    star,
    slash,
    arrow,
    dot,
    endOfFile
};

struct Token
{
    TokenType type = TokenType::endOfFile;
    std::string lexeme;
    int start = 0;
    int end = 0;
};

struct Lexer
{
    explicit Lexer(const std::string& s) : source(s) {}

    std::vector<Token> lex()
    {
        std::vector<Token> tokens;
        while (!isAtEnd())
        {
            const auto c = peek();
            if (std::isspace(static_cast<unsigned char>(c)))
            {
                advance();
                continue;
            }

            if (c == '/' && peekNext() == '/')
            {
                while (!isAtEnd() && peek() != '\n')
                    advance();
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(c)))
            {
                tokens.push_back(numberToken());
                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
            {
                tokens.push_back(identifierToken());
                continue;
            }

            const int start = pos;
            switch (c)
            {
                case '@':
                {
                    advance();
                    if (match('k'))
                        tokens.push_back({ TokenType::atControl, "@k", start, pos });
                    break;
                }
                case '(':
                    advance();
                    tokens.push_back({ TokenType::lParen, "(", start, pos });
                    break;
                case ')':
                    advance();
                    tokens.push_back({ TokenType::rParen, ")", start, pos });
                    break;
                case ',':
                    advance();
                    tokens.push_back({ TokenType::comma, ",", start, pos });
                    break;
                case ';':
                    advance();
                    tokens.push_back({ TokenType::semicolon, ";", start, pos });
                    break;
                case '=':
                    advance();
                    tokens.push_back({ TokenType::assign, "=", start, pos });
                    break;
                case '+':
                    advance();
                    tokens.push_back({ TokenType::plus, "+", start, pos });
                    break;
                case '-':
                    advance();
                    if (match('>'))
                        tokens.push_back({ TokenType::arrow, "->", start, pos });
                    else
                        tokens.push_back({ TokenType::minus, "-", start, pos });
                    break;
                case '*':
                    advance();
                    tokens.push_back({ TokenType::star, "*", start, pos });
                    break;
                case '/':
                    advance();
                    tokens.push_back({ TokenType::slash, "/", start, pos });
                    break;
                case '.':
                    advance();
                    tokens.push_back({ TokenType::dot, ".", start, pos });
                    break;
                default:
                    advance();
                    break;
            }
        }

        tokens.push_back({ TokenType::endOfFile, "", pos, pos });
        return tokens;
    }

    const std::string& source;
    int pos = 0;

    bool isAtEnd() const { return pos >= static_cast<int>(source.size()); }
    char peek() const { return source[static_cast<size_t>(pos)]; }
    char peekNext() const
    {
        const auto next = pos + 1;
        if (next >= static_cast<int>(source.size()))
            return '\0';
        return source[static_cast<size_t>(next)];
    }
    char advance() { return source[static_cast<size_t>(pos++)]; }
    bool match(const char expected)
    {
        if (isAtEnd() || source[static_cast<size_t>(pos)] != expected)
            return false;
        ++pos;
        return true;
    }

    Token numberToken()
    {
        const int start = pos;
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())))
            advance();
        if (!isAtEnd() && peek() == '.')
        {
            advance();
            while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())))
                advance();
        }
        return { TokenType::number, source.substr(static_cast<size_t>(start), static_cast<size_t>(pos - start)), start, pos };
    }

    Token identifierToken()
    {
        const int start = pos;
        while (!isAtEnd())
        {
            const auto c = peek();
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '~')
                break;
            advance();
        }
        return { TokenType::identifier, source.substr(static_cast<size_t>(start), static_cast<size_t>(pos - start)), start, pos };
    }
};

enum class ExprKind
{
    number,
    symbol,
    call,
    binary,
    pipeline,
    controlRate
};

struct Expr
{
    ExprKind kind = ExprKind::number;
    int start = 0;
    int end = 0;
    std::string text;
    std::string op;
    std::vector<std::unique_ptr<Expr>> args;
};

struct Stmt
{
    std::string binding;
    std::unique_ptr<Expr> expr;
    int start = 0;
    int end = 0;
};

class Parser
{
public:
    explicit Parser(std::vector<Token> toks) : tokens(std::move(toks)) {}

    std::vector<Stmt> parse()
    {
        std::vector<Stmt> stmts;
        while (!isAtEnd())
        {
            if (check(TokenType::endOfFile))
                break;
            if (auto stmt = parseStatement(); stmt.expr != nullptr)
                stmts.push_back(std::move(stmt));
            else
                synchronize();
        }
        return stmts;
    }

    const std::vector<Diagnostic>& diagnostics() const { return diags; }

private:
    std::vector<Token> tokens;
    std::vector<Diagnostic> diags;
    int current = 0;

    bool isAtEnd() const { return tokens[static_cast<size_t>(current)].type == TokenType::endOfFile; }
    const Token& peek() const { return tokens[static_cast<size_t>(current)]; }
    const Token& previous() const { return tokens[static_cast<size_t>(current - 1)]; }

    bool check(TokenType type) const
    {
        if (isAtEnd())
            return false;
        return peek().type == type;
    }

    const Token& advance()
    {
        if (!isAtEnd())
            ++current;
        return previous();
    }

    bool match(TokenType type)
    {
        if (!check(type))
            return false;
        advance();
        return true;
    }

    void synchronize()
    {
        while (!isAtEnd())
        {
            if (previous().type == TokenType::semicolon)
                return;
            advance();
        }
    }

    Stmt parseStatement()
    {
        Stmt stmt;
        stmt.start = peek().start;

        if (check(TokenType::identifier) && tokens[static_cast<size_t>(current + 1)].type == TokenType::assign)
        {
            stmt.binding = advance().lexeme;
            advance(); // =
            stmt.expr = parseExpression();
        }
        else
        {
            stmt.expr = parseExpression();
        }

        if (!match(TokenType::semicolon))
        {
            diags.push_back({ "Expected ';' at end of statement.", peek().start, peek().end });
            return {};
        }

        stmt.end = previous().end;
        return stmt;
    }

    std::unique_ptr<Expr> parseExpression()
    {
        return parsePipeline();
    }

    std::unique_ptr<Expr> parsePipeline()
    {
        auto lhs = parseAddition();
        while (match(TokenType::arrow))
        {
            auto rhs = parseCallOnly();
            if (rhs == nullptr)
            {
                diags.push_back({ "Expected function call on right side of pipeline.", previous().start, previous().end });
                return lhs;
            }
            auto p = std::make_unique<Expr>();
            p->kind = ExprKind::pipeline;
            p->start = lhs->start;
            p->end = rhs->end;
            p->args.push_back(std::move(lhs));
            p->args.push_back(std::move(rhs));
            lhs = std::move(p);
        }
        return lhs;
    }

    std::unique_ptr<Expr> parseAddition()
    {
        auto expr = parseMultiplication();
        while (match(TokenType::plus) || match(TokenType::minus))
        {
            const auto op = previous().lexeme;
            auto rhs = parseMultiplication();
            auto b = std::make_unique<Expr>();
            b->kind = ExprKind::binary;
            b->op = op;
            b->start = expr->start;
            b->end = rhs->end;
            b->args.push_back(std::move(expr));
            b->args.push_back(std::move(rhs));
            expr = std::move(b);
        }
        return expr;
    }

    std::unique_ptr<Expr> parseMultiplication()
    {
        auto expr = parsePostfix();
        while (match(TokenType::star) || match(TokenType::slash))
        {
            const auto op = previous().lexeme;
            auto rhs = parsePostfix();
            auto b = std::make_unique<Expr>();
            b->kind = ExprKind::binary;
            b->op = op;
            b->start = expr->start;
            b->end = rhs->end;
            b->args.push_back(std::move(expr));
            b->args.push_back(std::move(rhs));
            expr = std::move(b);
        }
        return expr;
    }

    std::unique_ptr<Expr> parsePostfix()
    {
        auto base = parsePrimary();
        while (match(TokenType::dot))
        {
            if (!match(TokenType::identifier))
            {
                diags.push_back({ "Expected method name after '.'.", peek().start, peek().end });
                return base;
            }
            const auto method = previous().lexeme;

            auto call = std::make_unique<Expr>();
            call->kind = ExprKind::call;
            call->text = method;
            call->start = base->start;
            call->args.push_back(std::move(base));

            if (match(TokenType::lParen))
            {
                if (!check(TokenType::rParen))
                {
                    do
                    {
                        call->args.push_back(parseExpression());
                    } while (match(TokenType::comma));
                }
                if (!match(TokenType::rParen))
                    diags.push_back({ "Expected ')' after method call.", peek().start, peek().end });
            }
            call->end = previous().end;
            base = std::move(call);
        }
        return base;
    }

    std::unique_ptr<Expr> parsePrimary()
    {
        if (match(TokenType::atControl))
        {
            auto x = std::make_unique<Expr>();
            x->kind = ExprKind::controlRate;
            x->start = previous().start;
            x->args.push_back(parsePrimary());
            x->end = x->args[0] != nullptr ? x->args[0]->end : previous().end;
            return x;
        }

        if (match(TokenType::number))
        {
            auto x = std::make_unique<Expr>();
            x->kind = ExprKind::number;
            x->text = previous().lexeme;
            x->start = previous().start;
            x->end = previous().end;
            return x;
        }

        if (check(TokenType::identifier))
        {
            if (tokens[static_cast<size_t>(current + 1)].type == TokenType::lParen)
                return parseCallOnly();

            auto x = std::make_unique<Expr>();
            x->kind = ExprKind::symbol;
            x->text = advance().lexeme;
            x->start = previous().start;
            x->end = previous().end;
            return x;
        }

        if (match(TokenType::lParen))
        {
            auto expr = parseExpression();
            if (!match(TokenType::rParen))
                diags.push_back({ "Expected ')' after expression.", peek().start, peek().end });
            return expr;
        }

        diags.push_back({ "Unexpected token.", peek().start, peek().end });
        return {};
    }

    std::unique_ptr<Expr> parseCallOnly()
    {
        if (!match(TokenType::identifier))
            return {};

        auto call = std::make_unique<Expr>();
        call->kind = ExprKind::call;
        call->text = previous().lexeme;
        call->start = previous().start;

        if (!match(TokenType::lParen))
        {
            diags.push_back({ "Expected '(' after function name.", peek().start, peek().end });
            return {};
        }

        if (!check(TokenType::rParen))
        {
            do
            {
                call->args.push_back(parseExpression());
            } while (match(TokenType::comma));
        }

        if (!match(TokenType::rParen))
            diags.push_back({ "Expected ')' after function arguments.", peek().start, peek().end });

        call->end = previous().end;
        return call;
    }
};

struct CompileContext
{
    ir::Graph graph;
    sync::SyncMap syncMap;
    std::vector<Diagnostic> diagnostics;
    std::unordered_map<std::string, std::string> bindingToNode;
    int generated = 1;

    std::string makeId(const std::string& seed)
    {
        return seed + "_" + std::to_string(generated++);
    }

    std::string addNode(const std::string& op, const std::string& label, const int start, const int end, const std::optional<double> literal = std::nullopt)
    {
        auto id = makeId(op);
        graph.nodes.push_back({ id, op, label, literal });
        syncMap.addRange(id, start, end);
        return id;
    }

    void connect(const std::string& from, const std::string& to, const int port)
    {
        graph.edges.push_back({ from, to, port });
    }
};

std::string canonicalOpFromCallName(const std::string& name)
{
    if (name == "dac~")
        return "out";
    if (name == "osc~")
        return "sin";
    if (name == "phasor~")
        return "saw";
    if (name == "tri~")
        return "tri";
    if (name == "noise~")
        return "noise";
    if (name == "lop~")
        return "lpf";
    if (name == "hip~")
        return "hpf";
    if (name == "lores~")
        return "lores";
    if (name == "bpf~")
        return "bpf";
    if (name == "svf~")
        return "svf";
    if (name == "freeverb~")
        return "freeverb";
    if (name == "plate~")
        return "plate";
    if (name == "reverb~")
        return "reverb";
    if (name == "fdn~")
        return "fdn";
    if (name == "convrev~")
        return "convrev";
    if (name == "delay~")
        return "delay";
    if (name == "delay")
        return "cdelay";
    if (name == "metro")
        return "metro";
    if (name == "apf~")
        return "apf";
    if (name == "comb~")
        return "comb";
    if (name == "comp~")
        return "comp_sig";
    if (name == "abs~")
        return "abs_sig";
    if (name == "min~")
        return "min_sig";
    if (name == "max~")
        return "max_sig";
    if (name == "and~")
        return "and_sig";
    if (name == "or~")
        return "or_sig";
    if (name == "xor~")
        return "xor_sig";
    if (name == "not~")
        return "not_sig";
    if (name == "clip~")
        return "clip";
    if (name == "tanh~")
        return "tanh";
    if (name == "slew~")
        return "slew";
    if (name == "sah~")
        return "sah";
    if (name == "sah")
        return "sah_c";
    if (name == "line")
        return "line";
    if (name == "line~")
        return "line_sig";
    if (name == "vline~")
        return "vline";
    if (name == "ad~")
        return "ad";
    if (name == "toggle")
        return "toggle";
    if (name == "select")
        return "select";
    if (name == "trigger" || name == "t")
        return "trigger";
    if (name == "pack")
        return "pack";
    if (name == "unpack")
        return "unpack";
    if (name == "snapshot~")
        return "snapshot";
    if (name == "pan~")
        return "pan";
    if (name == "env~")
        return "env";
    if (name == "peak~")
        return "peak";
    if (name == "mtof~")
        return "mtof_sig";
    if (name == "pow~")
        return "pow";
    if (name == "mod~")
        return "mod";
    return name;
}

std::string displayCallNameFromOp(const std::string& op)
{
    if (op == "sin")
        return "osc~";
    if (op == "saw")
        return "phasor~";
    if (op == "tri")
        return "tri~";
    if (op == "noise")
        return "noise~";
    if (op == "lpf")
        return "lop~";
    if (op == "hpf")
        return "hip~";
    if (op == "lores")
        return "lores~";
    if (op == "bpf")
        return "bpf~";
    if (op == "svf")
        return "svf~";
    if (op == "freeverb")
        return "freeverb~";
    if (op == "plate")
        return "plate~";
    if (op == "reverb")
        return "reverb~";
    if (op == "fdn")
        return "fdn~";
    if (op == "convrev")
        return "convrev~";
    if (op == "delay")
        return "delay~";
    if (op == "cdelay")
        return "delay";
    if (op == "metro")
        return "metro";
    if (op == "apf")
        return "apf~";
    if (op == "comb")
        return "comb~";
    if (op == "comp_sig")
        return "comp~";
    if (op == "abs_sig")
        return "abs~";
    if (op == "min_sig")
        return "min~";
    if (op == "max_sig")
        return "max~";
    if (op == "and_sig")
        return "and~";
    if (op == "or_sig")
        return "or~";
    if (op == "xor_sig")
        return "xor~";
    if (op == "not_sig")
        return "not~";
    if (op == "clip")
        return "clip~";
    if (op == "tanh")
        return "tanh~";
    if (op == "slew")
        return "slew~";
    if (op == "sah")
        return "sah~";
    if (op == "sah_c")
        return "sah";
    if (op == "line")
        return "line";
    if (op == "line_sig")
        return "line~";
    if (op == "vline")
        return "vline~";
    if (op == "ad")
        return "ad~";
    if (op == "toggle")
        return "toggle";
    if (op == "select")
        return "select";
    if (op == "trigger")
        return "trigger";
    if (op == "pack")
        return "pack";
    if (op == "unpack")
        return "unpack";
    if (op == "snapshot")
        return "snapshot~";
    if (op == "pan")
        return "pan~";
    if (op == "env")
        return "env~";
    if (op == "peak")
        return "peak~";
    if (op == "mtof_sig")
        return "mtof~";
    if (op == "pow")
        return "pow~";
    if (op == "mod")
        return "mod~";
    return op;
}

std::vector<double> parseLabelDefaults(const std::string& label)
{
    std::vector<double> vals;
    if (label.empty())
        return vals;

    std::istringstream iss(label);
    std::string tok;
    if (!(iss >> tok))
        return vals;

    while (iss >> tok)
    {
        char* end = nullptr;
        const auto v = std::strtod(tok.c_str(), &end);
        if (end != tok.c_str() && end != nullptr && *end == '\0')
            vals.push_back(v);
    }

    if (!vals.empty())
        return vals;

    char* end = nullptr;
    const auto v = std::strtod(label.c_str(), &end);
    if (end != label.c_str() && end != nullptr && *end == '\0')
        vals.push_back(v);
    return vals;
}

std::string formatDefaultNumber(const double v)
{
    std::ostringstream s;
    s << v;
    return s.str();
}

std::string callLabelWithNumericArgs(const Expr& callExpr)
{
    std::string label = callExpr.text;
    for (const auto& argPtr : callExpr.args)
    {
        if (argPtr == nullptr)
            continue;
        const auto& arg = *argPtr;
        if (arg.kind == ExprKind::number)
        {
            label += " ";
            label += arg.text;
        }
    }
    return label;
}

std::string compileExpr(CompileContext& ctx, const Expr& expr, bool forceControl)
{
    switch (expr.kind)
    {
        case ExprKind::number:
        {
            const auto value = std::strtod(expr.text.c_str(), nullptr);
            return ctx.addNode("constant", expr.text, expr.start, expr.end, value);
        }
        case ExprKind::symbol:
        {
            if (ctx.bindingToNode.contains(expr.text))
                return ctx.bindingToNode[expr.text];

            const auto op = forceControl ? "control" : "input";
            auto id = ctx.addNode(op, expr.text, expr.start, expr.end);
            ctx.bindingToNode[expr.text] = id;
            return id;
        }
        case ExprKind::controlRate:
        {
            if (expr.args.empty() || expr.args[0] == nullptr)
                return ctx.addNode("control", "control", expr.start, expr.end);
            return compileExpr(ctx, *expr.args[0], true);
        }
        case ExprKind::binary:
        {
            const auto lhs = compileExpr(ctx, *expr.args[0], forceControl);
            const auto rhs = compileExpr(ctx, *expr.args[1], forceControl);
            std::string op = forceControl ? "cadd" : "add";
            if (expr.op == "-")
                op = forceControl ? "csub" : "sub";
            else if (expr.op == "*")
                op = forceControl ? "cmul" : "mul";
            else if (expr.op == "/")
                op = forceControl ? "cdiv" : "div";
            const auto id = ctx.addNode(op, expr.op, expr.start, expr.end);
            ctx.connect(lhs, id, 0);
            ctx.connect(rhs, id, 1);
            return id;
        }
        case ExprKind::call:
        {
            const auto op = canonicalOpFromCallName(expr.text);
            const auto id = ctx.addNode(op, callLabelWithNumericArgs(expr), expr.start, expr.end);
            for (int i = 0; i < static_cast<int>(expr.args.size()); ++i)
            {
                const auto arg = compileExpr(ctx, *expr.args[static_cast<size_t>(i)], forceControl);
                ctx.connect(arg, id, i);
            }
            return id;
        }
        case ExprKind::pipeline:
        {
            const auto source = compileExpr(ctx, *expr.args[0], forceControl);
            const auto& rhs = *expr.args[1];
            if (rhs.kind != ExprKind::call)
            {
                ctx.diagnostics.push_back({ "Pipeline RHS must be a call.", rhs.start, rhs.end });
                return source;
            }
            const auto op = canonicalOpFromCallName(rhs.text);
            const auto id = ctx.addNode(op, callLabelWithNumericArgs(rhs), expr.start, expr.end);
            ctx.connect(source, id, 0);
            for (int i = 0; i < static_cast<int>(rhs.args.size()); ++i)
            {
                const auto arg = compileExpr(ctx, *rhs.args[static_cast<size_t>(i)], forceControl);
                ctx.connect(arg, id, i + 1);
            }
            return id;
        }
    }
    return ctx.addNode("input", "input", expr.start, expr.end);
}

bool replaceNodeId(ir::Graph& g, sync::SyncMap& map, const std::string& oldId, const std::string& newId)
{
    if (oldId == newId)
        return true;
    if (g.findNode(newId) != nullptr)
        return false;
    if (auto* n = g.findNode(oldId); n != nullptr)
        n->id = newId;
    else
        return false;
    for (auto& e : g.edges)
    {
        if (e.fromNodeId == oldId)
            e.fromNodeId = newId;
        if (e.toNodeId == oldId)
            e.toNodeId = newId;
    }
    for (auto& b : g.bindings)
        if (b.second == oldId)
            b.second = newId;
    auto ranges = map.ranges();
    map.clear();
    for (const auto& r : ranges)
        map.addRange(r.nodeId == oldId ? newId : r.nodeId, r.start, r.end);
    return true;
}

std::vector<const ir::Edge*> inputEdgesFor(const ir::Graph& graph, const std::string& nodeId)
{
    std::vector<const ir::Edge*> edges;
    for (const auto& e : graph.edges)
        if (e.toNodeId == nodeId)
            edges.push_back(&e);
    std::sort(edges.begin(), edges.end(), [](const auto* a, const auto* b) { return a->toPort < b->toPort; });
    return edges;
}

void sanitizeGraphReferences(ir::Graph& graph)
{
    std::unordered_set<std::string> ids;
    for (const auto& n : graph.nodes)
        ids.insert(n.id);

    graph.edges.erase(std::remove_if(graph.edges.begin(), graph.edges.end(), [&](const auto& e)
                     { return !ids.contains(e.fromNodeId) || !ids.contains(e.toNodeId); }),
                      graph.edges.end());

    for (auto it = graph.bindings.begin(); it != graph.bindings.end();)
    {
        if (!ids.contains(it->second))
            it = graph.bindings.erase(it);
        else
            ++it;
    }
}

std::string exprForNodeImpl(const ir::Graph& graph,
                            const std::string& nodeId,
                            std::unordered_map<std::string, std::string>& memo,
                            std::unordered_set<std::string>& inFlight)
{
    if (memo.contains(nodeId))
        return memo[nodeId];
    if (inFlight.contains(nodeId))
        return "0";

    inFlight.insert(nodeId);
    const auto* node = graph.findNode(nodeId);
    if (node == nullptr)
    {
        inFlight.erase(nodeId);
        return "0";
    }

    const auto ins = inputEdgesFor(graph, nodeId);
    if (node->op == "constant")
    {
        std::ostringstream s;
        s << (node->literal.has_value() ? *node->literal : 0.0);
        memo[nodeId] = s.str();
        inFlight.erase(nodeId);
        return memo[nodeId];
    }
    if (node->op == "input" || node->op == "control")
    {
        memo[nodeId] = node->label;
        inFlight.erase(nodeId);
        return memo[nodeId];
    }
    const auto spec = ir::opSpecFor(node->op);
    const auto defaults = parseLabelDefaults(node->label);
    auto defaultForPort = [&](const int port) -> std::string
    {
        if (port < 0)
            return {};
        if (defaults.empty())
            return {};
        if (port < static_cast<int>(defaults.size()) && defaults.size() == spec.inputs.size())
            return formatDefaultNumber(defaults[static_cast<size_t>(port)]);
        if (defaults.size() < spec.inputs.size() && !spec.inputs.empty())
        {
            const auto& first = spec.inputs.front().name;
            const auto trailing = first == "in" || first == "hot" || first == "a" || first == "left" || first == "trig";
            if (trailing)
            {
                const auto start = static_cast<int>(spec.inputs.size() - defaults.size());
                if (port >= start && port - start < static_cast<int>(defaults.size()))
                    return formatDefaultNumber(defaults[static_cast<size_t>(port - start)]);
                return {};
            }
        }
        if (port < static_cast<int>(defaults.size()))
            return formatDefaultNumber(defaults[static_cast<size_t>(port)]);
        return {};
    };
    auto argAt = [&](const int port) -> std::string
    {
        for (const auto* e : ins)
            if (e->toPort == port)
                return exprForNodeImpl(graph, e->fromNodeId, memo, inFlight);
        return defaultForPort(port);
    };

    if (node->op == "msg")
    {
        auto stored = defaultForPort(0);
        if (stored.empty())
            stored = "0";
        memo[nodeId] = "msg(" + stored + ")";
        inFlight.erase(nodeId);
        return memo[nodeId];
    }

    if ((node->op == "add" || node->op == "sub" || node->op == "mul" || node->op == "div") && (!argAt(0).empty() || !argAt(1).empty()))
    {
        auto a = argAt(0);
        auto b = argAt(1);
        if (a.empty())
            a = "0";
        if (b.empty())
            b = node->op == "mul" || node->op == "div" ? "1" : "0";
        const auto symbol = node->op == "add" ? "+" : node->op == "sub" ? "-" : node->op == "mul" ? "*" : "/";
        memo[nodeId] = "(" + a + " " + symbol + " " + b + ")";
        inFlight.erase(nodeId);
        return memo[nodeId];
    }

    const auto maxArgs = std::max(static_cast<int>(spec.inputs.size()), static_cast<int>(defaults.size()));
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(maxArgs));
    for (int i = 0; i < maxArgs; ++i)
    {
        auto a = argAt(i);
        if (!a.empty())
            args.push_back(a);
    }
    std::string line = displayCallNameFromOp(node->op) + "(";
    for (size_t i = 0; i < args.size(); ++i)
    {
        line += args[i];
        if (i + 1 < args.size())
            line += ", ";
    }
    line += ")";
    memo[nodeId] = line;
    inFlight.erase(nodeId);
    return memo[nodeId];
}

std::string exprForNode(const ir::Graph& graph, const std::string& nodeId, std::unordered_map<std::string, std::string>& memo)
{
    std::unordered_set<std::string> inFlight;
    return exprForNodeImpl(graph, nodeId, memo, inFlight);
}

bool isAutoBindingName(const std::string& name)
{
    if (name.size() < 2 || name[0] != 'n')
        return false;
    for (size_t i = 1; i < name.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(name[i])))
            return false;
    return true;
}

std::string opAliasBase(const std::string& op)
{
    if (op == "sin")
        return "osc";
    if (op == "saw")
        return "phasor";
    if (op == "tri")
        return "tri";
    if (op == "noise")
        return "noise";
    if (op == "bang")
        return "bang";
    if (op == "msg")
        return "msg";
    if (op == "floatatom")
        return "num";
    if (op == "out")
        return "dac";
    if (op == "mtof")
        return "mtof";
    if (op == "mtof_sig")
        return "mtofsig";
    return displayCallNameFromOp(op);
}

std::string compactIdentifier(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (const auto c : s)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (out.empty())
        out = "node";
    return out;
}

bool envVerbose()
{
    const auto* v = std::getenv("SCHISM_CODE_VERBOSE");
    if (v == nullptr)
        return false;
    const std::string s(v);
    return s == "1" || s == "true" || s == "TRUE" || s == "yes" || s == "YES";
}

std::string structureSignature(const ir::Graph& graph,
                               const std::string& nodeId,
                               std::unordered_map<std::string, std::string>& memo,
                               std::unordered_set<std::string>& inFlight)
{
    if (memo.contains(nodeId))
        return memo[nodeId];
    if (inFlight.contains(nodeId))
        return "cycle:" + nodeId;

    const auto* node = graph.findNode(nodeId);
    if (node == nullptr)
        return "missing";

    inFlight.insert(nodeId);
    auto s = node->op;
    if (node->op == "constant")
        s += ":" + std::to_string(node->literal.value_or(0.0));
    else if (node->op == "input" || node->op == "control")
        s += ":" + node->label;

    const auto inputs = inputEdgesFor(graph, nodeId);
    s += "(";
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        s += std::to_string(inputs[i]->toPort) + "=" + structureSignature(graph, inputs[i]->fromNodeId, memo, inFlight);
        if (i + 1 < inputs.size())
            s += ",";
    }
    s += ")";
    inFlight.erase(nodeId);
    memo[nodeId] = s;
    return s;
}

void preserveStableNodeIds(const ir::Graph& previous, ir::Graph& next, sync::SyncMap& map)
{
    std::unordered_set<std::string> usedOld;

    // Strongest hint: same binding name should keep prior root node identity.
    for (const auto& [binding, nextId] : next.bindings)
    {
        if (!previous.bindings.contains(binding))
            continue;
        const auto& oldId = previous.bindings.at(binding);
        if (usedOld.contains(oldId))
            continue;
        if (replaceNodeId(next, map, nextId, oldId))
            usedOld.insert(oldId);
    }

    std::unordered_map<std::string, std::string> prevSigMemo;
    std::unordered_map<std::string, std::string> nextSigMemo;
    std::unordered_set<std::string> prevFlight;
    std::unordered_set<std::string> nextFlight;

    std::unordered_map<std::string, std::vector<std::string>> prevBySignature;
    for (const auto& n : previous.nodes)
    {
        if (usedOld.contains(n.id))
            continue;
        prevBySignature[structureSignature(previous, n.id, prevSigMemo, prevFlight)].push_back(n.id);
    }

    for (const auto& n : next.nodes)
    {
        const auto sig = structureSignature(next, n.id, nextSigMemo, nextFlight);
        if (!prevBySignature.contains(sig))
            continue;
        auto& candidates = prevBySignature[sig];
        while (!candidates.empty() && usedOld.contains(candidates.back()))
            candidates.pop_back();
        if (candidates.empty())
            continue;
        const auto oldId = candidates.back();
        if (replaceNodeId(next, map, n.id, oldId))
            usedOld.insert(oldId);
    }
}
} // namespace

CompileResult compile(const std::string& source, const ir::Graph* previousGraph)
{
    CompileResult result;
    Lexer lexer(source);
    Parser parser(lexer.lex());
    auto stmts = parser.parse();
    result.diagnostics = parser.diagnostics();

    CompileContext ctx;

    for (const auto& stmt : stmts)
    {
        if (stmt.expr == nullptr)
            continue;
        const auto root = compileExpr(ctx, *stmt.expr, false);

        if (!stmt.binding.empty())
        {
            ctx.bindingToNode[stmt.binding] = root;
            ctx.graph.bindings[stmt.binding] = root;
        }
    }

    // Ensure explicit out call exists for audio output by creating one from last binding if missing.
    bool hasOut = false;
    for (const auto& n : ctx.graph.nodes)
        if (n.op == "out")
            hasOut = true;

    if (!hasOut && !ctx.graph.bindings.empty())
    {
        const auto& fallback = ctx.graph.bindings.begin()->second;
        const auto out = ctx.addNode("out", "out", 0, 0);
        ctx.connect(fallback, out, 0);
        ctx.connect(fallback, out, 1);
    }

    if (previousGraph != nullptr)
        preserveStableNodeIds(*previousGraph, ctx.graph, ctx.syncMap);

    sanitizeGraphReferences(ctx.graph);

    for (const auto& issue : ir::validateGraph(ctx.graph))
        ctx.diagnostics.push_back({ issue.message, 0, 0 });

    result.graph = std::move(ctx.graph);
    result.syncMap = std::move(ctx.syncMap);
    result.diagnostics.insert(result.diagnostics.end(), ctx.diagnostics.begin(), ctx.diagnostics.end());
    result.prettyPrinted = prettyPrint(result.graph);
    return result;
}

std::string prettyPrint(const ir::Graph& graph)
{
    return prettyPrint(graph, envVerbose());
}

std::string prettyPrint(const ir::Graph& graph, const bool verbose)
{
    std::ostringstream out;

    std::vector<std::pair<std::string, std::string>> bindings;
    for (const auto& b : graph.bindings)
        bindings.push_back(b);
    std::unordered_map<std::string, int> nodeOrder;
    for (int i = 0; i < static_cast<int>(graph.nodes.size()); ++i)
        nodeOrder[graph.nodes[static_cast<size_t>(i)].id] = i;
    std::sort(bindings.begin(), bindings.end(), [&](const auto& a, const auto& b)
              {
                  const auto ai = nodeOrder.contains(a.second) ? nodeOrder[a.second] : std::numeric_limits<int>::max();
                  const auto bi = nodeOrder.contains(b.second) ? nodeOrder[b.second] : std::numeric_limits<int>::max();
                  if (ai != bi)
                      return ai < bi;
                  return a.first < b.first;
              });

    std::unordered_set<std::string> boundIds;
    for (const auto& [name, nodeId] : bindings)
        boundIds.insert(nodeId);

    auto feedsAnotherBoundNode = [&](const std::string& nodeId)
    {
        for (const auto& e : graph.edges)
            if (e.fromNodeId == nodeId && boundIds.contains(e.toNodeId))
                return true;
        return false;
    };

    std::unordered_map<std::string, std::string> memo;
    std::unordered_map<std::string, std::string> emittedBindingByNodeId;
    std::vector<std::pair<std::string, std::string>> emittedBindings;
    bool emittedAnyBinding = false;
    for (const auto& [name, nodeId] : bindings)
    {
        if (isAutoBindingName(name) && feedsAnotherBoundNode(nodeId))
            continue;
        emittedBindingByNodeId[nodeId] = name;
        emittedBindings.push_back({ name, nodeId });
        emittedAnyBinding = true;
    }

    // Fallback: if all bindings were pruned (e.g. all auto and chained), keep the final binding visible.
    if (!emittedAnyBinding && !bindings.empty())
    {
        const auto& [name, nodeId] = bindings.back();
        emittedBindingByNodeId[nodeId] = name;
        emittedBindings.push_back({ name, nodeId });
    }

    auto refForNode = [&](const std::string& nodeId)
    {
        if (emittedBindingByNodeId.contains(nodeId))
            return emittedBindingByNodeId.at(nodeId);
        return exprForNode(graph, nodeId, memo);
    };

    auto triggerSourceFor = [&](const std::string& nodeId) -> std::string
    {
        const auto* n = graph.findNode(nodeId);
        if (n == nullptr)
            return {};
        if (!(n->op == "msg" || n->op == "random" || n->op == "bang" || n->op == "cdelay" || n->op == "metro"))
            return {};
        for (const auto& e : graph.edges)
            if (e.toNodeId == nodeId && e.toPort == 0)
                return refForNode(e.fromNodeId);
        return {};
    };

    enum class LineClass
    {
        event,
        control,
        audio
    };

    auto classify = [&](const ir::Node& n) -> LineClass
    {
        if (n.op == "bang" || n.op == "msg" || n.op == "random" || n.op == "cdelay" || n.op == "metro")
            return LineClass::event;
        const auto rate = ir::opSpecFor(n.op).outputRate;
        return rate == ir::PortRate::control ? LineClass::control : LineClass::audio;
    };

    std::unordered_map<std::string, int> aliasCounters;
    std::vector<std::string> eventLines;
    std::vector<std::string> controlLines;
    std::vector<std::string> audioLines;
    for (const auto& [name, nodeId] : emittedBindings)
    {
        const auto* n = graph.findNode(nodeId);
        if (n == nullptr)
            continue;

        std::string line = name + " = " + exprForNode(graph, nodeId, memo) + ";";
        if (verbose && isAutoBindingName(name))
        {
            auto base = compactIdentifier(opAliasBase(n->op));
            auto& idx = aliasCounters[base];
            ++idx;
            line += " // alias: " + base + std::to_string(idx);
        }
        const auto trigSrc = triggerSourceFor(nodeId);
        if (!trigSrc.empty())
            line += " // trig <- " + trigSrc;
        if (verbose)
            line += " // nodeId: " + nodeId;

        const auto cls = classify(*n);
        if (cls == LineClass::event)
            eventLines.push_back(line);
        else if (cls == LineClass::control)
            controlLines.push_back(line);
        else
            audioLines.push_back(line);
    }

    auto writeSection = [&](const std::string& title, const std::vector<std::string>& lines)
    {
        if (lines.empty())
            return;
        out << "// " << title << "\n";
        for (const auto& l : lines)
            out << l << "\n";
        out << "\n";
    };

    writeSection("EVENT / CLOCK", eventLines);
    writeSection("CONTROL", controlLines);
    writeSection("AUDIO", audioLines);
    out << "// OUTPUT\n";

    for (const auto& n : graph.nodes)
    {
        if (n.op != "out")
            continue;
        const auto ins = inputEdgesFor(graph, n.id);
        std::string leftExpr = "0";
        std::string rightExpr = {};
        bool leftConnected = false;
        bool rightConnected = false;
        for (const auto* e : ins)
        {
            if (e->toPort == 0)
            {
                leftExpr = refForNode(e->fromNodeId);
                leftConnected = true;
            }
            else if (e->toPort == 1)
            {
                rightExpr = refForNode(e->fromNodeId);
                rightConnected = true;
            }
        }
        if (!rightConnected)
            rightExpr = leftExpr;

        out << "dac~(" << leftExpr << ", " << rightExpr << "); // dac L <- "
            << (leftConnected ? leftExpr : std::string("(unconnected)"))
            << ", R <- " << (rightConnected ? rightExpr : std::string("(mirrored L)")) << "\n";
    }
    return out.str();
}
} // namespace duodsp::text
