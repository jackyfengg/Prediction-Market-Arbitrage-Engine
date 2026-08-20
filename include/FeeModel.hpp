#pragma once

class FeeModel {
public:
    explicit FeeModel(double feeRate = 0.0);

    double calculateFee(double notional) const;

private:
    double _feeRate;
};