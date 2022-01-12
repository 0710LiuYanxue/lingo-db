
#include "mlir/Dialect/RelAlg/IR/RelAlgDialect.h"
#include <mlir/Transforms/InliningUtils.h>

#include "mlir/Dialect/DB/IR/DBTypes.h"
#include "mlir/Dialect/RelAlg/IR/RelAlgOps.h"
#include "mlir/IR/DialectImplementation.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace mlir;
using namespace mlir::relalg;

struct RelalgInlinerInterface : public DialectInlinerInterface {
   using DialectInlinerInterface::DialectInlinerInterface;

   //===--------------------------------------------------------------------===//
   // Analysis Hooks
   //===--------------------------------------------------------------------===//

   /// All call operations within toy can be inlined.
   bool isLegalToInline(Operation* call, Operation* callable,
                        bool wouldBeCloned) const final override{
      return true;
   }

   /// All operations within toy can be inlined.
   bool isLegalToInline(Operation*, Region*, bool,
                        BlockAndValueMapping&) const final override{
      return true;
   }
   virtual bool isLegalToInline(Region *dest, Region *src, bool wouldBeCloned,
                                BlockAndValueMapping &valueMapping) const override{
      return true;
   }
};
#define GET_ATTRDEF_CLASSES
#include "mlir/Dialect/RelAlg/IR/RelAlgOpsAttributes.cpp.inc"
void RelAlgDialect::initialize() {
   addOperations<
#define GET_OP_LIST
#include "mlir/Dialect/RelAlg/IR/RelAlgOps.cpp.inc"
      >();
   addTypes<
#define GET_TYPEDEF_LIST
#include "mlir/Dialect/RelAlg/IR/RelAlgOpsTypes.cpp.inc"
      >();
   addAttributes<
#define GET_ATTRDEF_LIST
#include "mlir/Dialect/RelAlg/IR/RelAlgOpsAttributes.cpp.inc"
      >();
   addInterfaces<RelalgInlinerInterface>();
   addAttributes<mlir::relalg::RelationalAttributeDefAttr>();
   addAttributes<mlir::relalg::RelationalAttributeRefAttr>();
   addAttributes<mlir::relalg::SortSpecificationAttr>();
   relationalAttributeManager.setContext(getContext());

}

/// Parse a type registered to this dialect.
::mlir::Type RelAlgDialect::parseType(::mlir::DialectAsmParser& parser) const {
   if (!parser.parseOptionalKeyword("tuplestream")) {
      return mlir::relalg::TupleStreamType::get(parser.getBuilder().getContext());
   }
   if (!parser.parseOptionalKeyword("tuple")) {
      return mlir::relalg::TupleType::get(parser.getBuilder().getContext());
   }
   return mlir::Type();
}

/// Print a type registered to this dialect.
void RelAlgDialect::printType(::mlir::Type type,
                              ::mlir::DialectAsmPrinter& os) const {
   if (type.isa<mlir::relalg::TupleStreamType>()) {
      os << "tuplestream";
   }
   if (type.isa<mlir::relalg::TupleType>()) {
      os << "tuple";
   }
}
::mlir::Attribute
RelAlgDialect::parseAttribute(::mlir::DialectAsmParser& parser,
                              ::mlir::Type type) const {
   if (!parser.parseOptionalKeyword("attr_def")) {
      std::string name;
      if (parser.parseLBrace() || parser.parseOptionalString(&name) || parser.parseRBrace())
         return mlir::Attribute();
      return parser.getBuilder().getContext()->getLoadedDialect<RelAlgDialect>()->getRelationalAttributeManager().createDef(SymbolRefAttr::get(parser.getContext(),name));
   }else {
      Attribute attr;
      llvm::StringRef mnemonic;
      if (!parser.parseKeyword(&mnemonic)) {
         auto parseResult = generatedAttributeParser(parser, mnemonic, type, attr);
         if (parseResult.hasValue())
            return attr;
      }
   }
   return mlir::Attribute();
}
void RelAlgDialect::printAttribute(::mlir::Attribute attr,
                                   ::mlir::DialectAsmPrinter& os) const {
   if (auto attrDef = attr.dyn_cast_or_null<mlir::relalg::RelationalAttributeDefAttr>()) {
      os << "attr_def(\\\"" << attrDef.getName() << "\\\")";
   }else{
      if (succeeded(generatedAttributePrinter(attr, os)))
         return;
   }
}

::mlir::Attribute mlir::relalg::TableMetaDataAttr::parse(::mlir::AsmParser& parser, ::mlir::Type type) {
   if(parser.parseLess()) return Attribute();
   StringAttr attr;
   if(parser.parseAttribute(attr))return Attribute();
   if(parser.parseGreater())
      return Attribute();
   return mlir::relalg::TableMetaDataAttr::get(parser.getContext(), runtime::TableMetaData::deserialize(attr.str()));
}
void mlir::relalg::TableMetaDataAttr::print(::mlir::AsmPrinter& printer) const {
   printer<<"<"<<getMeta()->serialize()<<">";
}
#include "mlir/Dialect/RelAlg/IR/RelAlgOpsDialect.cpp.inc"
