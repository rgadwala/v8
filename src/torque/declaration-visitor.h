// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_DECLARATION_VISITOR_H_
#define V8_TORQUE_DECLARATION_VISITOR_H_

#include <set>
#include <string>

#include "src/base/macros.h"
#include "src/torque/declarations.h"
#include "src/torque/file-visitor.h"
#include "src/torque/global-context.h"
#include "src/torque/scope.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class DeclarationVisitor : public FileVisitor {
 public:
  explicit DeclarationVisitor(GlobalContext& global_context)
      : FileVisitor(global_context),
        scope_(declarations(), global_context.GetDefaultModule()) {}

  void Visit(Ast* ast) {
    Visit(ast->default_module());
    DrainSpecializationQueue();
  }

  void Visit(Expression* expr);
  void Visit(Statement* stmt);
  void Visit(Declaration* decl);

  void Visit(ModuleDeclaration* decl) {
    ScopedModuleActivator activator(this, decl->GetModule());
    Declarations::ModuleScopeActivator scope(declarations(), decl->GetModule());
    for (Declaration* child : decl->declarations) Visit(child);
  }
  void Visit(DefaultModuleDeclaration* decl) {
    decl->SetModule(global_context_.GetDefaultModule());
    Visit(implicit_cast<ModuleDeclaration*>(decl));
  }
  void Visit(ExplicitModuleDeclaration* decl) {
    decl->SetModule(global_context_.GetModule(decl->name));
    Visit(implicit_cast<ModuleDeclaration*>(decl));
  }

  void Visit(IdentifierExpression* expr);
  void Visit(NumberLiteralExpression* expr) {}
  void Visit(StringLiteralExpression* expr) {}
  void Visit(CallExpression* expr);
  void Visit(ElementAccessExpression* expr) {
    Visit(expr->array);
    Visit(expr->index);
  }
  void Visit(FieldAccessExpression* expr) { Visit(expr->object); }
  void Visit(BlockStatement* expr) {
    Declarations::NodeScopeActivator scope(declarations(), expr);
    for (Statement* stmt : expr->statements) Visit(stmt);
  }
  void Visit(ExpressionStatement* stmt) { Visit(stmt->expression); }
  void Visit(TailCallStatement* stmt) { Visit(stmt->call); }
  void Visit(TypeDeclaration* decl);

  void Visit(TypeAliasDeclaration* decl) {
    const Type* type = declarations()->GetType(decl->type);
    type->AddAlias(decl->name);
    declarations()->DeclareType(decl->name, type);
  }

  Builtin* BuiltinDeclarationCommon(BuiltinDeclaration* decl, bool external,
                                    const Signature& signature);

  void Visit(ExternalBuiltinDeclaration* decl, const Signature& signature,
             Statement* body) {
    BuiltinDeclarationCommon(decl, true, signature);
  }

  void Visit(ExternalRuntimeDeclaration* decl, const Signature& sig,
             Statement* body);
  void Visit(ExternalMacroDeclaration* decl, const Signature& sig,
             Statement* body);
  void Visit(TorqueBuiltinDeclaration* decl, const Signature& signature,
             Statement* body);
  void Visit(TorqueMacroDeclaration* decl, const Signature& signature,
             Statement* body);

  void Visit(CallableNode* decl, const Signature& signature, Statement* body);

  void Visit(StandardDeclaration* decl);
  void Visit(GenericDeclaration* decl);
  void Visit(SpecializationDeclaration* decl);
  void Visit(ReturnStatement* stmt);

  void Visit(DebugStatement* stmt) {}
  void Visit(AssertStatement* stmt) {
    bool do_check = !stmt->debug_only;
#if defined(DEBUG)
    do_check = true;
#endif
    if (do_check) DeclareExpressionForBranch(stmt->expression);
  }

  void Visit(VarDeclarationStatement* stmt);

  void Visit(ExternConstDeclaration* decl) {
    // TODO(szuend): When module-wide const bindings are available, only
    //               constexpr types should be allowed here.
    declarations()->DeclareConstant(
        decl->name, declarations()->GetType(decl->type), decl->literal);
  }

  void Visit(LogicalOrExpression* expr);
  void Visit(LogicalAndExpression* expr);
  void DeclareExpressionForBranch(Expression* node);

  void Visit(ConditionalExpression* expr);
  void Visit(IfStatement* stmt);
  void Visit(WhileStatement* stmt);
  void Visit(ForOfLoopStatement* stmt);

  void Visit(AssignmentExpression* expr) {
    MarkLocationModified(expr->location);
    Visit(expr->location);
    Visit(expr->value);
  }

  void Visit(BreakStatement* stmt) {}
  void Visit(ContinueStatement* stmt) {}
  void Visit(GotoStatement* expr) {}
  void Visit(ForLoopStatement* stmt);

  void Visit(IncrementDecrementExpression* expr) {
    MarkLocationModified(expr->location);
    Visit(expr->location);
  }

  void Visit(TryLabelStatement* stmt);
  void GenerateHeader(std::string& file_name);

 private:
  struct LiveAndChanged {
    std::set<const Variable*> live;
    std::set<const Variable*> changed;
  };

  void PushControlSplit() {
    LiveAndChanged live_and_changed;
    live_and_changed.live = declarations()->GetLiveVariables();
    live_and_changed_variables_.push_back(live_and_changed);
  }

  std::set<const Variable*> PopControlSplit() {
    auto result = live_and_changed_variables_.back().changed;
    live_and_changed_variables_.pop_back();
    return result;
  }

  void MarkLocationModified(Expression* location);
  bool MarkVariableModified(const Variable* variable);
  void DeclareSignature(const Signature& signature);
  void DeclareSpecializedTypes(const SpecializationKey& key);

  void Specialize(const SpecializationKey& key, CallableNode* callable,
                  const CallableNodeSignature* signature,
                  Statement* body) override;

  Declarations::ModuleScopeActivator scope_;
  std::vector<Builtin*> torque_builtins_;
  std::vector<LiveAndChanged> live_and_changed_variables_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_DECLARATION_VISITOR_H_
