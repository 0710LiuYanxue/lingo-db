#ifndef MLIR_DB_PASSES_H
#define MLIR_DB_PASSES_H

#include "mlir/Pass/Pass.h"
#include <memory>

namespace mlir {
    namespace db {
        std::unique_ptr<Pass> createLowerToStdPass();
    }// end namespace db
}// end namespace mlir

#endif// MLIR_DB_PASSES_H