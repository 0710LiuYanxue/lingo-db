#include "mlir/Conversion/RelAlgToDB/NLJoinTranslator.h"
#include "mlir/Conversion/RelAlgToDB/Translator.h"
#include "mlir/Dialect/DB/IR/DBOps.h"
#include "mlir/Dialect/RelAlg/IR/RelAlgOps.h"
#include "mlir/Dialect/SCF/SCF.h"
#include "mlir/Dialect/util/UtilOps.h"

#include <mlir/Conversion/RelAlgToDB/HashJoinTranslator.h>
#include <mlir/IR/BlockAndValueMapping.h>

class NLSemiJoinTranslator : public mlir::relalg::JoinImpl {
   public:
   NLSemiJoinTranslator(mlir::relalg::SemiJoinOp innerJoinOp) : mlir::relalg::JoinImpl(innerJoinOp, innerJoinOp.right(), innerJoinOp.left()) {
   }

   virtual void handleLookup(mlir::Value matched, mlir::Value /*marker*/, mlir::relalg::TranslatorContext& context, mlir::OpBuilder& builder) override {
      builder.create<mlir::db::SetFlag>(loc, matchFoundFlag, matched);
   }

   void beforeLookup(mlir::relalg::TranslatorContext& context, mlir::OpBuilder& builder) override {
      matchFoundFlag = builder.create<mlir::db::CreateFlag>(loc, mlir::db::FlagType::get(builder.getContext()));
   }
   void afterLookup(mlir::relalg::TranslatorContext& context, mlir::OpBuilder& builder) override {
      mlir::Value matchFound = builder.create<mlir::db::GetFlag>(loc, mlir::db::BoolType::get(builder.getContext()), matchFoundFlag);
      translator->handlePotentialMatch(builder, context, matchFound);
   }
   virtual ~NLSemiJoinTranslator() {}
};
class MHashSemiJoinTranslator : public mlir::relalg::JoinImpl {
   public:
   MHashSemiJoinTranslator(mlir::relalg::SemiJoinOp innerJoinOp) : mlir::relalg::JoinImpl(innerJoinOp, innerJoinOp.left(), innerJoinOp.right(), true) {
   }

   virtual void handleLookup(mlir::Value matched, mlir::Value markerPtr, mlir::relalg::TranslatorContext& context, mlir::OpBuilder& builder) override {
      auto beforeBuilderValues = translator->getRequiredBuilderValues(context);
      auto ifOp = builder.create<mlir::db::IfOp>(
         loc, translator->getRequiredBuilderTypes(context), matched, [&](mlir::OpBuilder& builder1, mlir::Location) {
            auto const1 = builder1.create<mlir::arith::ConstantOp>(loc, builder1.getIntegerType(64), builder1.getI64IntegerAttr(1));
            auto markerBefore = builder1.create<mlir::memref::AtomicRMWOp>(loc, builder1.getIntegerType(64), mlir::arith::AtomicRMWKind::assign, const1, markerPtr, mlir::ValueRange{});
            {
               auto zero = builder1.create<mlir::arith::ConstantOp>(loc, markerBefore.getType(), builder1.getIntegerAttr(markerBefore.getType(), 0));
               auto isZero = builder1.create<mlir::arith::CmpIOp>(loc, mlir::arith::CmpIPredicate::eq, markerBefore, zero);
               auto isZeroDB = builder1.create<mlir::db::TypeCastOp>(loc, mlir::db::BoolType::get(builder1.getContext()), isZero);
               translator->handlePotentialMatch(builder,context,isZeroDB);
            }
            builder1.create<mlir::db::YieldOp>(loc, translator->getRequiredBuilderValues(context)); },
         translator->requiredBuilders.empty() ? translator->noBuilder : [&](mlir::OpBuilder& builder2, mlir::Location) { builder2.create<mlir::db::YieldOp>(loc, beforeBuilderValues); });
      translator->setRequiredBuilderValues(context, ifOp.getResults());
   }

   virtual ~MHashSemiJoinTranslator() {}
};

std::unique_ptr<mlir::relalg::Translator> mlir::relalg::Translator::createSemiJoinTranslator(mlir::relalg::SemiJoinOp joinOp) {
   if (joinOp->hasAttr("impl")) {
      if (auto impl = joinOp->getAttr("impl").dyn_cast_or_null<mlir::StringAttr>()) {
         if (impl.getValue() == "hash") {
            return (std::unique_ptr<mlir::relalg::Translator>) std::make_unique<mlir::relalg::HashJoinTranslator>(std::make_shared<NLSemiJoinTranslator>(joinOp));
         }
         if (impl.getValue() == "markhash") {
            return (std::unique_ptr<mlir::relalg::Translator>) std::make_unique<mlir::relalg::HashJoinTranslator>(std::make_shared<MHashSemiJoinTranslator>(joinOp));
         }
      }
   }
   return (std::unique_ptr<mlir::relalg::Translator>) std::make_unique<mlir::relalg::NLJoinTranslator>(std::make_shared<NLSemiJoinTranslator>(joinOp));
}