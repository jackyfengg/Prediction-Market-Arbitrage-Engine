#pragma once

// Models the exchange fee schedule as a simple rate applied to notional.
// Keeping this behind an interface means the detector/simulator never
// hardcode a fee formula, and different schedules can be plugged in later.
class FeeModel {
public:
    explicit FeeModel(double feeRate = 0.0);

    // Fee charged on a trade of the given notional (price * quantity).
    double calculateFee(double notional) const;

private:
    double _feeRate;
};
