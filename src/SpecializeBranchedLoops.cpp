#include "SpecializeClampedRamps.h"
#include "ExprUsesVar.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "LinearSolve.h"
#include "Simplify.h"
#include "Substitute.h"

namespace Halide {
namespace Internal {

namespace {

bool is_inequality(Expr cond) {
  const LT *lt = cond.as<LT>();
  const GT *gt = cond.as<GT>();
  const LE *le = cond.as<LE>();
  const GE *ge = cond.as<GE>();

  return lt || gt || le || ge;
}

}

struct Branch {
    Expr min;
    Expr extent;
    Stmt then_case;
    Stmt else_case;
};

struct BranchedLoop {
    const For *op;
    bool has_branch;
    Branch branch;
};

class CheckBranched : public IRVisitor {
public:
    Scope<Expr>* scope;
    std::vector<BranchedLoop>& loops;

    CheckBranched(std::vector<BranchedLoop>& l, Scope<Expr>* s) : scope(s), loops(l) {}

private:
    using IRVisitor::visit;

    void visit(const IfThenElse *op) {
        for (size_t i = 0; i < loops.size(); ++i) {
            const For* for_loop = loops[i].op;
            Expr cond = op->condition;
            if (is_inequality(cond) && expr_uses_var(cond, for_loop->name)) {
                Expr solve = solve_for_linear_variable(cond, for_loop->name, scope);
                if (!solve.same_as(cond)) {
                    const LT *lt = solve.as<LT>();
                    const GT *gt = solve.as<GT>();
                    const LE *le = solve.as<LE>();
                    const GE *ge = solve.as<GE>();

                    if (lt) {
                        Expr lhs = lt->b;
                        Branch b = {for_loop->min, simplify(lhs - for_loop->min),
                                    op->then_case, op->else_case};
                        loops[i].branch = b;
                        loops[i].has_branch = true;
                    } else if (gt) {
                        Expr lhs = gt->b;
                        Branch b = {simplify(lhs + 1), for_loop->extent,
                                    op->then_case, op->else_case};
                        loops[i].branch = b;
                        loops[i].has_branch = true;
                    } else if (le) {
                        Expr lhs = le->b;
                        Branch b = {for_loop->min, simplify(lhs - for_loop->min),
                                    op->then_case, op->else_case};
                        loops[i].branch = b;
                        loops[i].has_branch = true;
                    } else /*if (ge)*/ {
                        Expr lhs = ge->b;
                        Branch b = {lhs, for_loop->extent,
                                    op->then_case, op->else_case};
                        loops[i].branch = b;
                        loops[i].has_branch = true;
                    }
                }
            }
        }
    }

    void visit(const LetStmt *op) {
        scope->push(op->name, op->value);
        op->body.accept(this);
        scope->pop(op->name);
    }

};

class SpecializeLoopBranches : public IRMutator {
public:
    SpecializeLoopBranches() : check_loop(loops, &scope) {}
private:
    Scope<Expr> scope;
    std::vector<BranchedLoop> loops;
    CheckBranched check_loop;

    using IRVisitor::visit;

    void visit(const For *op) {
        BranchedLoop loop;
        loop.op = op;
        loop.has_branch = false;
        loops.push_back(loop);

        Stmt body = mutate(op->body);
        body.accept(&check_loop);

        if (loops.back().has_branch) {
            const Branch& branch = loops.back().branch;

            Stmt first;
            Stmt second;

            if (branch.min.same_as(op->min)) {
                first  = For::make(op->name, branch.min, branch.extent, op->for_type, branch.then_case);

                if (branch.else_case.defined()) {
                    Expr second_min = simplify(branch.min + branch.extent);
                    Expr second_extent = simplify(op->extent - branch.extent);
                    second = For::make(op->name, second_min, second_extent, op->for_type, branch.else_case);
                }
            } else {
                if (branch.else_case.defined()) {
                    Expr first_extent = simplify(branch.min - op->min);
                    first  = For::make(op->name, op->min, first_extent, op->for_type, branch.else_case);
                }

                second = For::make(op->name, branch.min, branch.extent, op->for_type, branch.then_case);
            }

            if (first.defined() && second.defined()) {
                stmt = Block::make(first, second);
            } else if (first.defined()) {
                stmt = first;
            } else {
                stmt = second;
            }
        } else {
            stmt = op;
        }

        loops.pop_back();
    }

    void visit(const LetStmt *op) {
        scope.push(op->name, op->value);
        Stmt body = mutate(op->body);
        scope.pop(op->name);

        if (body.same_as(op->body)) {
            stmt = op;
        } else {
            stmt = LetStmt::make(op->name, op->value, body);
        }
    }
};

Stmt specialize_branched_loops(Stmt s) {
    Stmt spec_stmt = s;
    SpecializeLoopBranches specialize;
    do {
        s = spec_stmt;
        spec_stmt = specialize.mutate(s);
    } while(!spec_stmt.same_as(s));

    return spec_stmt;
}

}
}
