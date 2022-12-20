#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/DB/IR/DBOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

#include <iostream>

#include "mlir/Dialect/RelAlg/Passes.h"
#include "mlir/IR/BlockAndValueMapping.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

namespace {
#include "EliminateNulls.inc"

class WrapWithNullCheck : public mlir::RewritePattern {
   public:
   WrapWithNullCheck(mlir::MLIRContext* context) : RewritePattern(MatchAnyOpTypeTag(), mlir::PatternBenefit(1), context) {}
   mlir::LogicalResult match(mlir::Operation* op) const override {
      if (op->getNumResults() > 1) return mlir::failure();
      if (op->getNumResults() == 1 && !op->getResultTypes()[0].isa<mlir::db::NullableType>()) return mlir::failure();
      auto needsWrapInterface = mlir::dyn_cast_or_null<mlir::db::NeedsNullWrap>(op);
      if (!needsWrapInterface) return mlir::failure();
      if (!needsWrapInterface.needsNullWrap()) return mlir::failure();
      if (llvm::any_of(op->getOperands(), [](mlir::Value v) { return v.getType().isa<mlir::db::NullableType>(); })) {
         return mlir::success();
      }
      return mlir::failure();
   }

   void rewrite(mlir::Operation* op, mlir::PatternRewriter& rewriter) const override {
      rewriter.setInsertionPoint(op);
      mlir::Value isAnyNull;
      for (auto operand : op->getOperands()) {
         if (operand.getType().isa<mlir::db::NullableType>()) {
            auto isCurrNull = rewriter.create<mlir::db::IsNullOp>(op->getLoc(), operand);
            if (isAnyNull) {
               isAnyNull = rewriter.create<mlir::arith::OrIOp>(op->getLoc(), isAnyNull, isCurrNull);
            } else {
               isAnyNull = isCurrNull;
            }
         }
      }

      auto supInvVal = mlir::dyn_cast_or_null<mlir::db::SupportsInvalidValues>(op);
      if (supInvVal && supInvVal.supportsInvalidValues()) {
         mlir::BlockAndValueMapping mapping;
         for (auto operand : op->getOperands()) {
            if (operand.getType().isa<mlir::db::NullableType>()) {
               mapping.map(operand, rewriter.create<mlir::db::NullableGetVal>(op->getLoc(), operand));
            }
         }
         auto* cloned = rewriter.clone(*op, mapping);
         if (op->getNumResults() == 1) {
            cloned->getResult(0).setType(getBaseType(cloned->getResult(0).getType()));
            rewriter.replaceOpWithNewOp<mlir::db::AsNullableOp>(op, op->getResultTypes()[0], cloned->getResult(0), isAnyNull);
         } else {
            rewriter.eraseOp(op);
         }
         return;
      } else {
         rewriter.replaceOpWithNewOp<mlir::scf::IfOp>(
            op, op->getResultTypes(), isAnyNull, [&](mlir::OpBuilder& b, mlir::Location loc) {
               if(op->getNumResults()==1){
                  mlir::Value nullResult=b.create<mlir::db::NullOp>(op->getLoc(),op->getResultTypes()[0]);
                  b.create<mlir::scf::YieldOp>(loc,nullResult);
               }else{
                  b.create<mlir::scf::YieldOp>(loc);
               } }, [&](mlir::OpBuilder& b, mlir::Location loc) {
               mlir::BlockAndValueMapping mapping;
               for (auto operand : op->getOperands()) {
                  if (operand.getType().isa<mlir::db::NullableType>()) {
                     mapping.map(operand,b.create<mlir::db::NullableGetVal>(op->getLoc(),operand));
                  }
               }
               auto *cloned=b.clone(*op,mapping);
               if(op->getNumResults()==1){
                  cloned->getResult(0).setType(getBaseType(cloned->getResult(0).getType()));
                  mlir::Value nullResult=b.create<mlir::db::AsNullableOp>(op->getLoc(),op->getResultTypes()[0],cloned->getResult(0));
                  b.create<mlir::scf::YieldOp>(loc,nullResult);
               }else{
                  b.create<mlir::scf::YieldOp>(loc);
               } });
      }
   }
};
class SimplifySortComparePattern : public mlir::RewritePattern {
   public:
   SimplifySortComparePattern(mlir::MLIRContext* context)
      : RewritePattern(mlir::db::SortCompare::getOperationName(), 1, context) {}
   mlir::LogicalResult matchAndRewrite(mlir::Operation* op, mlir::PatternRewriter& rewriter) const override {
      auto sortCompareOp = mlir::cast<mlir::db::SortCompare>(op);
      auto loc = op->getLoc();
      if (!sortCompareOp.getLeft().getType().isa<mlir::db::NullableType>()) return mlir::failure();
      mlir::Value isLeftNull = rewriter.create<mlir::db::IsNullOp>(loc, sortCompareOp.getLeft());
      mlir::Value isRightNull = rewriter.create<mlir::db::IsNullOp>(loc, sortCompareOp.getRight());
      mlir::Value isAnyNull = rewriter.create<mlir::arith::OrIOp>(loc, isLeftNull, isRightNull);
      mlir::Value bothNullable = rewriter.create<mlir::arith::AndIOp>(loc, isLeftNull, isRightNull);
      mlir::Value zero = rewriter.create<mlir::arith::ConstantIntOp>(loc, 0, 8);
      mlir::Value minus1 = rewriter.create<mlir::arith::ConstantIntOp>(loc, -1, 8);
      mlir::Value one = rewriter.create<mlir::arith::ConstantIntOp>(loc, 1, 8);
      rewriter.replaceOpWithNewOp<mlir::scf::IfOp>(
         op, op->getResultTypes(), isAnyNull, [&](mlir::OpBuilder& b, mlir::Location loc) {
            mlir::Value res = b.create<mlir::arith::SelectOp>(loc, isRightNull, minus1, one);
            res = b.create<mlir::arith::SelectOp>(loc, bothNullable, zero, res);
            b.create<mlir::scf::YieldOp>(loc, res); }, [&](mlir::OpBuilder& b, mlir::Location loc) {
            mlir::Value left = b.create<mlir::db::NullableGetVal>(loc, sortCompareOp.getLeft());
            mlir::Value right = b.create<mlir::db::NullableGetVal>(loc, sortCompareOp.getRight());
            mlir::Value res=b.create<mlir::db::SortCompare>(loc,left,right);
            b.create<mlir::scf::YieldOp>(loc,res); });
      return mlir::success(true);
   }
};
class SimplifyCompareISAPattern : public mlir::RewritePattern {
   public:
   SimplifyCompareISAPattern(mlir::MLIRContext* context)
      : RewritePattern(mlir::db::CmpOp::getOperationName(), 1, context) {}
   mlir::LogicalResult matchAndRewrite(mlir::Operation* op, mlir::PatternRewriter& rewriter) const override {
      auto cmpOp = mlir::cast<mlir::db::CmpOp>(op);
      if (cmpOp.getPredicate() != mlir::db::DBCmpPredicate::isa) return mlir::failure();
      auto isLeftNullable = cmpOp.getLeft().getType().isa<mlir::db::NullableType>();
      auto isRightNullable = cmpOp.getRight().getType().isa<mlir::db::NullableType>();
      auto loc = op->getLoc();
      if (isLeftNullable && isRightNullable) {
         mlir::Value isLeftNull = rewriter.create<mlir::db::IsNullOp>(loc, cmpOp.getLeft());
         mlir::Value isRightNull = rewriter.create<mlir::db::IsNullOp>(loc, cmpOp.getRight());
         mlir::Value isAnyNull = rewriter.create<mlir::arith::OrIOp>(loc, isLeftNull, isRightNull);
         rewriter.replaceOpWithNewOp<mlir::scf::IfOp>(
            op, op->getResultTypes(), isAnyNull, [&](mlir::OpBuilder& b, mlir::Location loc) {
               mlir::Value bothNull = rewriter.create<mlir::arith::AndIOp>(loc, isLeftNull, isRightNull);
               b.create<mlir::scf::YieldOp>(loc, bothNull); }, [&](mlir::OpBuilder& b, mlir::Location loc) {
               mlir::Value left = b.create<mlir::db::NullableGetVal>(loc, cmpOp.getLeft());
               mlir::Value right = b.create<mlir::db::NullableGetVal>(loc, cmpOp.getRight());
               mlir::Value res=b.create<mlir::db::CmpOp>(loc,mlir::db::DBCmpPredicate::eq,left,right);
               b.create<mlir::scf::YieldOp>(loc,res); });
      } else if (isLeftNullable || isRightNullable) {
         mlir::Value isNull=rewriter.create<mlir::db::IsNullOp>(loc, isLeftNullable?cmpOp.getLeft():cmpOp.getRight());
         rewriter.replaceOpWithNewOp<mlir::scf::IfOp>(
            op, op->getResultTypes(), isNull, [&](mlir::OpBuilder& b, mlir::Location loc) {
               mlir::Value falseVal = rewriter.create<mlir::arith::ConstantIntOp>(loc, 0, rewriter.getI1Type());
               b.create<mlir::scf::YieldOp>(loc, falseVal); }, [&](mlir::OpBuilder& b, mlir::Location loc) {
               mlir::Value left=cmpOp.getLeft();
               mlir::Value right=cmpOp.getRight();
               if(isLeftNullable) {
                  left = b.create<mlir::db::NullableGetVal>(loc, left);
               }
               if(isRightNullable) {
                  right = b.create<mlir::db::NullableGetVal>(loc, right);
               }
               mlir::Value res=b.create<mlir::db::CmpOp>(loc,mlir::db::DBCmpPredicate::eq,left,right);
               b.create<mlir::scf::YieldOp>(loc,res); });
      } else {
         rewriter.replaceOpWithNewOp<mlir::db::CmpOp>(op, mlir::db::DBCmpPredicate::eq, cmpOp.getLeft(), cmpOp.getRight());
      }
      return mlir::success();
   }
};
//Pattern that optimizes the join order
class EliminateNulls : public mlir::PassWrapper<EliminateNulls, mlir::OperationPass<mlir::ModuleOp>> {
   virtual llvm::StringRef getArgument() const override { return "eliminate-nulls"; }
   void getDependentDialects(mlir::DialectRegistry& registry) const override {
      registry.insert<mlir::scf::SCFDialect>();
   }

   public:
   MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(EliminateNulls)
   void runOnOperation() override {
      //transform "standalone" aggregation functions
      {
         mlir::RewritePatternSet patterns(&getContext());
         //patterns.insert<EliminateNullCmp>(&getContext());
         patterns.insert<EliminateDeriveTruthNonNullable>(&getContext());
         patterns.insert<EliminateDeriveTruthNullable>(&getContext());
         patterns.insert<SimplifySortComparePattern>(&getContext());
         patterns.insert<SimplifyCompareISAPattern>(&getContext());
         //patterns.insert<SimplifyNullableCondSkip>(&getContext());
         patterns.insert<WrapWithNullCheck>(&getContext());
         if (mlir::applyPatternsAndFoldGreedily(getOperation().getRegion(), std::move(patterns)).failed()) {
            assert(false && "should not happen");
         }
      }
   }
};
} // end anonymous namespace

namespace mlir::db {

std::unique_ptr<Pass> createEliminateNullsPass() { return std::make_unique<EliminateNulls>(); }

} // end namespace mlir::db
