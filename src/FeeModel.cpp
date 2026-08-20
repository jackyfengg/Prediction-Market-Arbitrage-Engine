#include "FeeModel.hpp"

FeeModel::FeeModel(double feeRate)
    : _feeRate(feeRate)
{
}

double FeeModel::calculateFee(double notional) const {
    return notional * _feeRate;
}