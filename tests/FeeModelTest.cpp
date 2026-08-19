#include <gtest/gtest.h>

#include "FeeModel.hpp"

TEST(FeeModelTest, ZeroNotionalChargesNothing) {
    FeeModel feeModel(0.01);

    EXPECT_DOUBLE_EQ(feeModel.calculateFee(0.0), 0.0);
}

TEST(FeeModelTest, ZeroRateChargesNothing) {
    FeeModel feeModel(0.0);

    EXPECT_DOUBLE_EQ(feeModel.calculateFee(100.0), 0.0);
    EXPECT_DOUBLE_EQ(feeModel.calculateFee(10000.0), 0.0);
}

TEST(FeeModelTest, AppliesRateToNotional) {
    FeeModel feeModel(0.01);

    EXPECT_DOUBLE_EQ(feeModel.calculateFee(100.0), 1.0);
}

TEST(FeeModelTest, SmallTrade) {
    FeeModel feeModel(0.001);

    EXPECT_DOUBLE_EQ(feeModel.calculateFee(0.90), 0.0009);
}

TEST(FeeModelTest, LargeTrade) {
    FeeModel feeModel(0.002);

    EXPECT_DOUBLE_EQ(feeModel.calculateFee(5000.0), 10.0);
}
