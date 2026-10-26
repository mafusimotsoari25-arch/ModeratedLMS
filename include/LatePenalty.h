#pragma once
class ILatePenaltyStrategy {
public:
    virtual ~ILatePenaltyStrategy() = default;
    virtual double finalMark(double rawMark, double maximumMark, int daysLate) const = 0;
};
class StandardLatePenalty final : public ILatePenaltyStrategy {
public:
    double finalMark(double rawMark, double maximumMark, int daysLate) const override;
};
