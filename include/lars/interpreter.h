#pragma once

#include "parser.h"

#include <functional>

namespace lars {
  
  struct InterpreterError: std::exception {
    std::shared_ptr<SyntaxTree> tree;
    mutable std::string buffer;
    InterpreterError(const std::shared_ptr<SyntaxTree> &t):tree(t){}
    const char * what()const noexcept override;
  };

  template <class R, typename ... Args> class Interpreter {
  public:
    
    struct Expression;
    using Callback = std::function<R(const Expression &e, Args... args)>;
    
    struct Expression{
      struct iterator: public std::iterator<std::input_iterator_tag, Expression>{
        const Expression &parent;
        size_t idx;
        iterator(const Expression &p, size_t i):parent(p),idx(i){}
        iterator & operator++(){ idx++; return *this; }
        Expression operator*()const{ return parent[idx]; }
        bool operator!=(const iterator &other)const{ return other.idx != idx || &other.parent != &parent; }
      };
      
      const Interpreter<R, Args...> &interpreter;
      std::shared_ptr<SyntaxTree> syntaxTree;
      
      auto size()const{ return syntaxTree->inner.size(); }
      auto string()const{ return syntaxTree->string(); }
      auto position()const{ return syntaxTree->begin; }
      auto length()const{ return syntaxTree->length(); }
      
      Expression operator[](size_t idx)const{ return interpreter.interpret(syntaxTree->inner[idx]); }
      iterator begin()const{ return iterator(*this,0); }
      iterator end()const{ return iterator(*this,size()); }

      template <class R2, typename ... Args2> auto evaluateBy(const Interpreter<R2, Args2...> &interpreter, Args2... args)const{
        return interpreter.evaluate(syntaxTree, args...);
      }
      
      R evaluate(Args ... args)const{
        auto it = interpreter.evaluators.find(syntaxTree->rule.get());
        if (it == interpreter.evaluators.end()) {
          if (interpreter.defaultEvaluator) {
            return interpreter.defaultEvaluator(*this, args...);
          }
          throw InterpreterError(syntaxTree);
        }
        return it->second(*this, args...);
      }
      
    };
    

  private:
    std::unordered_map<peg::Rule *, Callback> evaluators;
    
  public:
    
    Callback defaultEvaluator = [](const Expression &e, Args... args){
      if(e.size() == 1) { return e[0].evaluate(args...); }
      throw InterpreterError(e.syntaxTree);
    };

    std::shared_ptr<peg::Rule> makeRule(const std::string_view &name, const peg::GrammarNode::Shared &node, const Callback &callback){
      auto rule = std::make_shared<peg::Rule>(name, node);
      setEvaluator(rule, callback);
      return rule;
    }
    
    std::shared_ptr<peg::Rule> makeRule(const std::string &name, const std::shared_ptr<peg::Rule> &rule, const Callback &callback){
      return makeRule(name, peg::GrammarNode::Rule(rule), callback);
    }

    void setEvaluator(const std::shared_ptr<peg::Rule> &rule, const Callback &callback){
      evaluators[rule.get()] = callback;
    }
    
    Expression interpret(const std::shared_ptr<SyntaxTree> &tree) const {
      return Expression{*this, tree};
    }
    
    R evaluate(const std::shared_ptr<SyntaxTree> &tree, Args ... args) const {
      return interpret(tree).evaluate(args...);
    }
    
  };
  
  struct SyntaxError: std::exception {
    std::shared_ptr<SyntaxTree> outerTree;
    std::shared_ptr<SyntaxTree> getErrorTree()const;

    mutable std::string buffer;
    SyntaxError(const std::shared_ptr<SyntaxTree> &t):outerTree(t){}
    const char * what()const noexcept override;
  };
  
  template <class R, typename ... Args> struct Program {
    using Expression = typename Interpreter<R, Args...>::Expression;
    
    Parser parser;
    Interpreter<R, Args...> interpreter;
    
    R interpret(const std::shared_ptr<SyntaxTree> &tree, Args ... args) const {
      if (!tree->valid) { throw SyntaxError(tree); }
      return interpreter.interpret(tree).evaluate(args...);
    }
    
    R run(const std::string_view &str, Args ... args) const {
      auto parsed = parser.parse(str);
      if (parsed->end < str.size()) { throw SyntaxError(parsed); }
      return interpret(parsed, args...);
    }
  };
  
}
