// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "dense_dot_product_function.h"
#include "dense_tensor.h"
#include "dense_tensor_view.h"
#include <vespa/eval/eval/operation.h>
#include <vespa/eval/eval/value.h>
#include <vespa/eval/tensor/tensor.h>

namespace vespalib::tensor {

using eval::ValueType;
using eval::TensorFunction;
using eval::as;
using eval::Aggr;
using namespace eval::tensor_function;
using namespace eval::operation;

namespace {

TypedCells getCellsRef(const eval::Value &value) {
    const DenseTensorView &denseTensor = static_cast<const DenseTensorView &>(value);
    return denseTensor.cellsRef();
}

struct CallDotProduct {
    template<typename LCT, typename RCT>
    static double
    call(const ConstArrayRef<LCT> &lhs, const ConstArrayRef<RCT> &rhs,
         size_t numCells, hwaccelrated::IAccelrated *hw_accelerator);
};

template<> double
CallDotProduct::call<double, double>(const ConstArrayRef<double> &lhs, const ConstArrayRef<double> &rhs,
                                     size_t numCells, hwaccelrated::IAccelrated *hw_accelerator)
{
    return hw_accelerator->dotProduct(lhs.cbegin(), rhs.cbegin(), numCells);
}

template<> double
CallDotProduct::call<float, float>(const ConstArrayRef<float> &lhs, const ConstArrayRef<float> &rhs,
                                   size_t numCells,  hwaccelrated::IAccelrated *hw_accelerator)
{
    return hw_accelerator->dotProduct(lhs.cbegin(), rhs.cbegin(), numCells);
}

template<> double
CallDotProduct::call<float, double>(const ConstArrayRef<float> &lhs, const ConstArrayRef<double> &rhs,
                                    size_t numCells, hwaccelrated::IAccelrated *)
{
    double result = 0.0;
    for (size_t i = 0; i < numCells; ++i) {
        result += lhs[i] * rhs[i];
    }
    return result;
}

template<> double
CallDotProduct::call<double, float>
(const ConstArrayRef<double> &lhs, const ConstArrayRef<float> &rhs,
 size_t numCells, hwaccelrated::IAccelrated *)
{
    double result = 0.0;
    for (size_t i = 0; i < numCells; ++i) {
        result += lhs[i] * rhs[i];
    }
    return result;
}

void my_dot_product_op(eval::InterpretedFunction::State &state, uint64_t param) {
    auto *hw_accelerator = (hwaccelrated::IAccelrated *)(param);
    TypedCells lhsCells = getCellsRef(state.peek(1));
    TypedCells rhsCells = getCellsRef(state.peek(0));
    size_t numCells = std::min(lhsCells.size, rhsCells.size);
    double result = dispatch_2<CallDotProduct>(lhsCells, rhsCells, numCells, hw_accelerator);
    state.pop_pop_push(state.stash.create<eval::DoubleValue>(result));
}

} // namespace vespalib::tensor::<unnamed>

DenseDotProductFunction::DenseDotProductFunction(const eval::TensorFunction &lhs_in,
                                                 const eval::TensorFunction &rhs_in)
    : eval::tensor_function::Op2(eval::ValueType::double_type(), lhs_in, rhs_in),
      _hwAccelerator(hwaccelrated::IAccelrated::getAccelrator())
{
}

eval::InterpretedFunction::Instruction
DenseDotProductFunction::compile_self(Stash &) const
{
    return eval::InterpretedFunction::Instruction(my_dot_product_op, (uint64_t)(_hwAccelerator.get()));
}

bool
DenseDotProductFunction::compatible_types(const ValueType &res, const ValueType &lhs, const ValueType &rhs)
{
    if (lhs.cell_type() != ValueType::CellType::DOUBLE ||
        rhs.cell_type() != ValueType::CellType::DOUBLE)
    {
        return false; // non-double cell types not supported
    }
    return (res.is_double() && lhs.is_dense() && (rhs == lhs));
}

const TensorFunction &
DenseDotProductFunction::optimize(const eval::TensorFunction &expr, Stash &stash)
{
    auto reduce = as<Reduce>(expr);
    if (reduce && (reduce->aggr() == Aggr::SUM)) {
        auto join = as<Join>(reduce->child());
        if (join && (join->function() == Mul::f)) {
            const TensorFunction &lhs = join->lhs();
            const TensorFunction &rhs = join->rhs();
            if (compatible_types(expr.result_type(), lhs.result_type(), rhs.result_type())) {
                return stash.create<DenseDotProductFunction>(lhs, rhs);
            }
        }
    }
    return expr;
}

} // namespace vespalib::tensor
