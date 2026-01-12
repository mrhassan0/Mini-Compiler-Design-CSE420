#ifndef AST_H
#define AST_H

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>

using namespace std;

class ASTNode {
public:
    virtual ~ASTNode() {}
    virtual string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp, int& temp_count, int& label_count) const = 0;
};

// Expression node types

class ExprNode : public ASTNode {
protected:
    string node_type; // Type info (int, float, void, etc.) propagated during parsing
public:
    ExprNode(string type) : node_type(type) {}
    virtual string get_type() const { return node_type; }
};

// Variable node (for ID references)

class VarNode : public ExprNode {
private:
    string name; 
    ExprNode* index; // Array subscript expression, nullptr when not indexing
    inline static bool force_fresh_load = false; 
    inline static map<string,string> last_access; 

public:
    VarNode(string name, string type, ExprNode* idx = nullptr) // Build a scalar or array reference
        : ExprNode(type), name(name), index(idx) {}
    
    ~VarNode() { if(index) delete index; } 
    
    bool has_index() const { return index != nullptr; } 
    
    string generate_index_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                              int& temp_count, int& label_count) const {
        if (!index) return ""; 
        return index->generate_code(outcode, symbol_to_temp, temp_count, label_count); 
    }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        string temp; 
        if (!has_index() && !force_fresh_load && symbol_to_temp.count(name)) {
            temp = symbol_to_temp[name]; // Reuse cached temp for simple variable
            temp_count++; 
        } else {
            temp = "t" + to_string(temp_count++); // Allocate a fresh temp
            if (!has_index()) {
                outcode << temp << " = " << name << endl; 
            } else {
                string idx_temp = generate_index_code(outcode, symbol_to_temp, temp_count, label_count); 
                outcode << temp << " = " << name << "[" << idx_temp << "]" << endl; 
            }
            symbol_to_temp[name] = temp; 
        }
        last_access[name] = temp; 
        return temp; 
    }
    
    string get_name() const { return name; } 
    
    static void set_force_fresh(bool v) { force_fresh_load = v; } 
    static void clear_last_access() { last_access.clear(); } 
    static const map<string,string>& get_last_access() { return last_access; } 
};

// Constant node

class ConstNode : public ExprNode {
private:
    string value;

public:
    ConstNode(string val, string type) : ExprNode(type), value(val) {}
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        string temp = "t" + to_string(temp_count++);
        outcode << temp << " = " << value << endl;
        return temp;
    }
};

// Binary operation node

class BinaryOpNode : public ExprNode {
private:
    string op; 
    ExprNode* left; 
    ExprNode* right; 

public:
    BinaryOpNode(string op, ExprNode* left, ExprNode* right, string result_type)
        : ExprNode(result_type), op(op), left(left), right(right) {} // Capture operator and operands
    
    ~BinaryOpNode() {
        delete left; 
        delete right; 
    }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        string l = left->generate_code(outcode, symbol_to_temp, temp_count, label_count); 
        string r = right->generate_code(outcode, symbol_to_temp, temp_count, label_count); 
        string temp = "t" + to_string(temp_count++); 
        outcode << temp << " = " << l << " " << op << " " << r << endl; 
        return temp;
    }
};

// Unary operation node

class UnaryOpNode : public ExprNode {
private:
    string op;
    ExprNode* expr;

public:
    UnaryOpNode(string op, ExprNode* expr, string result_type)
        : ExprNode(result_type), op(op), expr(expr) {}
    
    ~UnaryOpNode() { delete expr; }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        string val = expr->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        string temp = "t" + to_string(temp_count++);
        outcode << temp << " = " << op << val << endl;
        return temp;
    }
};

// Assignment node

class AssignNode : public ExprNode {
private:
    VarNode* lhs;
    ExprNode* rhs;

public:
    AssignNode(VarNode* lhs, ExprNode* rhs, string result_type)
        : ExprNode(result_type), lhs(lhs), rhs(rhs) {}
    
    ~AssignNode() {
        delete lhs;
        delete rhs;
    }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        string rval = rhs->generate_code(outcode, symbol_to_temp, temp_count, label_count);

        if (lhs->has_index()) {
                string idx = lhs->generate_index_code(outcode, symbol_to_temp, temp_count, label_count);
                outcode << lhs->get_name() << "[" << idx << "] = " << rval << endl;
            } else {
                outcode << lhs->get_name() << " = " << rval << endl;
            symbol_to_temp[lhs->get_name()] = rval; 
            }

            return rval;
        }
    };

// Statement node types

class StmtNode : public ASTNode {
public:
    virtual string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                                int& temp_count, int& label_count) const = 0;
};

// Expression statement node

class ExprStmtNode : public StmtNode {
private:
    ExprNode* expr; // Expression wrapped as a statement

public:
    ExprStmtNode(ExprNode* e) : expr(e) {} // Store the expression
    ~ExprStmtNode() { if(expr) delete expr; } 
    
    ExprNode* get_expr() const { return expr; } // Accessor for the wrapped expression
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        if (expr) { 
            expr->generate_code(outcode, symbol_to_temp, temp_count, label_count); // Evaluate expression, ignore result
        }
        return "";
    }
};

// Block (compound statement) node

class BlockNode : public StmtNode {
private:
    vector<StmtNode*> statements;

public:
    ~BlockNode() {
        for (auto stmt : statements) {
            delete stmt;
        }
    }
    
    void add_statement(StmtNode* stmt) { // Append a child statement to the block
        if (stmt) statements.push_back(stmt); 
    }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        for (auto stmt : statements) { // Emit code for each contained statement in order
            if (stmt) stmt->generate_code(outcode, symbol_to_temp, temp_count, label_count); 
        }
        return "";
    }
};

// If statement node

class IfNode : public StmtNode {
private:
    ExprNode* condition;
    StmtNode* then_block;
    StmtNode* else_block; // nullptr if no else part

public:
    IfNode(ExprNode* cond, StmtNode* then_stmt, StmtNode* else_stmt = nullptr)
        : condition(cond), then_block(then_stmt), else_block(else_stmt) {}
    
    ~IfNode() {
        delete condition;
        delete then_block;
        if (else_block) delete else_block;
    }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        string cond_temp = condition->generate_code(outcode, symbol_to_temp, temp_count, label_count);

        string true_label = "L" + to_string(label_count++);
        string false_label = "L" + to_string(label_count++);
        string end_label = "L" + to_string(label_count++);

        outcode << "if " << cond_temp << " goto " << true_label << endl;
        outcode << "goto " << false_label << endl;

        outcode << true_label << ":" << endl;
        if (then_block) then_block->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        outcode << "goto " << end_label << endl;

        outcode << false_label << ":" << endl;
        if (else_block) {
            else_block->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        }
        outcode << end_label << ":" << endl;

        return "";
    }
};

// While statement node

class WhileNode : public StmtNode {
private:
    ExprNode* condition;
    StmtNode* body;

public:
    WhileNode(ExprNode* cond, StmtNode* body_stmt)
        : condition(cond), body(body_stmt) {}
    
    ~WhileNode() {
        delete condition;
        delete body;
    }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        string start_label = "L" + to_string(label_count++);
        string body_label = "L" + to_string(label_count++);
        string end_label = "L" + to_string(label_count++);

        outcode << start_label << ":" << endl;
        string cond_temp = condition->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        outcode << "if " << cond_temp << " goto " << body_label << endl;
        outcode << "goto " << end_label << endl;

        outcode << body_label << ":" << endl;
        if (body) body->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        outcode << "goto " << start_label << endl;
        outcode << end_label << ":" << endl;

        return "";
    }
};

// For statement node

class ForNode : public StmtNode {
private:
    ASTNode* init; 
    ASTNode* condition; 
    ExprNode* update; // Update expression run each iteration
    StmtNode* body; 

public:
    ForNode(ASTNode* init_node, ASTNode* cond_node, ExprNode* update_expr, StmtNode* body_stmt)
        : init(init_node), condition(cond_node), update(update_expr), body(body_stmt) {}
    
    ~ForNode() {
        if (init) delete init; 
        if (condition) delete condition;
        if (update) delete update; 
        delete body; 
    }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        if (init) { 
            if (auto s = dynamic_cast<StmtNode*>(init)) s->generate_code(outcode, symbol_to_temp, temp_count, label_count); 
            else if (auto e = dynamic_cast<ExprNode*>(init)) e->generate_code(outcode, symbol_to_temp, temp_count, label_count); 
        }

        string start_label = "L" + to_string(label_count++); 
        string body_label = "L" + to_string(label_count++); 
        string end_label = "L" + to_string(label_count++); 
        outcode << start_label << ":" << endl; 
        string cond_temp = "1"; // Default true if no condition
        if (condition) { 
            VarNode::clear_last_access(); 
            VarNode::set_force_fresh(true); 
            if (auto es = dynamic_cast<ExprStmtNode*>(condition)) {
                if (es->get_expr()) cond_temp = es->get_expr()->generate_code(outcode, symbol_to_temp, temp_count, label_count); 
                else es->generate_code(outcode, symbol_to_temp, temp_count, label_count); //cond temp=1 , es=null
            } else if (auto e = dynamic_cast<ExprNode*>(condition)) {
                cond_temp = e->generate_code(outcode, symbol_to_temp, temp_count, label_count); 
            } else if (auto s = dynamic_cast<StmtNode*>(condition)) {
                s->generate_code(outcode, symbol_to_temp, temp_count, label_count); 
            }
            VarNode::set_force_fresh(false); 
            symbol_to_temp.clear();
            for (auto& kv : VarNode::get_last_access()) {
                symbol_to_temp[kv.first] = kv.second; // Copy last-access temps into current cache
            }
        }
        outcode << "if " << cond_temp << " goto " << body_label << endl; 
        outcode << "goto " << end_label << endl; 

        outcode << body_label << ":" << endl; 
        if (body) body->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        if (update) update->generate_code(outcode, symbol_to_temp, temp_count, label_count); // Emit update step
        outcode << "goto " << start_label << endl; 
        outcode << end_label << ":" << endl; 

        return "";
    }
};

// Return statement node

class ReturnNode : public StmtNode {
private:
    ExprNode* expr;

public:
    ReturnNode(ExprNode* e) : expr(e) {}
    ~ReturnNode() { if (expr) delete expr; }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        if (expr) {
            string val = expr->generate_code(outcode, symbol_to_temp, temp_count, label_count);
            outcode << "return " << val << endl;
        } else {
            outcode << "return" << endl;
        }
        return "";
    }
};

// Declaration node

class DeclNode : public StmtNode {
private:
    string type;
    vector<pair<string, int>> vars; // Variable name and array size (0 for regular vars)

public:
    DeclNode(string t) : type(t) {}
    
    void add_var(string name, int array_size = 0) {
        vars.push_back(make_pair(name, array_size));
    }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        for (auto& v : vars) {
            outcode << "// Declaration: " << type << " " << v.first;
            if (v.second > 0) outcode << "[" << v.second << "]"; //arraytype
            outcode << endl;
        }
        return "";
    }
    
    string get_type() const { return type; }
    const vector<pair<string, int>>& get_vars() const { return vars; }
};

// Function declaration node

class FuncDeclNode : public ASTNode {
private:
    string return_type; 
    string name; 
    vector<pair<string, string>> params; 
    BlockNode* body; 

public:
    FuncDeclNode(string ret_type, string n) : return_type(ret_type), name(n), body(nullptr) {} 
    ~FuncDeclNode() { if (body) delete body; } 
    
    void add_param(string type, string name) { 
        params.push_back(make_pair(type, name));
    }
    
    void set_body(BlockNode* b) { // Attach the parsed body
        body = b;
    }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        outcode << "// Function: " << return_type << " " << name << "("; // Header comment
        for (size_t i = 0; i < params.size(); i++) { // List parameters
            outcode << params[i].first << " " << params[i].second;
            if (i + 1 < params.size()) outcode << ", ";
        }
        outcode << ")" << endl;

        // New function scope for temp mapping
        symbol_to_temp.clear(); 

        if (body) body->generate_code(outcode, symbol_to_temp, temp_count, label_count); 

        outcode << endl; 
        return "";
    }
};

// Helper class for function arguments

class ArgumentsNode : public ASTNode {
private:
    vector<ExprNode*> args;

public:
    ~ArgumentsNode() {
        // Don't delete args here - they'll be transferred to FuncCallNode
    }
    
    void add_argument(ExprNode* arg) {
        if (arg) args.push_back(arg);
    }
    
    ExprNode* get_argument(int index) const {
        if (index >= 0 && index < args.size()) {
            return args[index];
        }
        return nullptr;
    }
    
    size_t size() const {
        return args.size();
    }
    
    const vector<ExprNode*>& get_arguments() const {
        return args;
    }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        // This node doesn't generate code directly
        return "";
    }
};

// Function call node

class FuncCallNode : public ExprNode {
private:
    string func_name;
    vector<ExprNode*> arguments;

public:
    FuncCallNode(string name, string result_type)
        : ExprNode(result_type), func_name(name) {}
    
    ~FuncCallNode() {
        for (auto arg : arguments) {
            delete arg;
        }
    }
    
    void add_argument(ExprNode* arg) {
        if (arg) arguments.push_back(arg);
    }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        for (auto arg : arguments) {
            string arg_temp;
            if (auto v = dynamic_cast<VarNode*>(arg)) {
                arg_temp = "t" + to_string(temp_count++);
                if (v->has_index()) {
                    string idx_temp = v->generate_index_code(outcode, symbol_to_temp, temp_count, label_count);
                    outcode << arg_temp << " = " << v->get_name() << "[" << idx_temp << "]" << endl;
                } else {
                    outcode << arg_temp << " = " << v->get_name() << endl;
                }
            } else {
                arg_temp = arg->generate_code(outcode, symbol_to_temp, temp_count, label_count);
            }
            outcode << "param " << arg_temp << endl;
        }

        if (node_type == "void") { //print returns nothing
            outcode << "call " << func_name << ", " << arguments.size() << endl;
            return "";
        }

        string temp = "t" + to_string(temp_count++);
        outcode << temp << " = call " << func_name << ", " << arguments.size() << endl;
        return temp;
    }
};

// Program node (root of AST)

class ProgramNode : public ASTNode { 
private:
    vector<ASTNode*> units;

public:
    ~ProgramNode() {
        for (auto unit : units) {
            delete unit;
        }
    }
    
    void add_unit(ASTNode* unit) {
        if (unit) units.push_back(unit);
    }
    
    string generate_code(ofstream& outcode, map<string, string>& symbol_to_temp,
                        int& temp_count, int& label_count) const override {
        for (auto unit : units) {
            if (unit) unit->generate_code(outcode, symbol_to_temp, temp_count, label_count);
        }
        return "";
    }
};

#endif // AST_H
