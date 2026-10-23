#include "LatePenalty.h"
#include <algorithm>
#include <stdexcept>

double StandardLatePenalty::finalMark(double rawMark, double maximumMark, int daysLate) const {
    if (maximumMark <= 0.0) throw std::invalid_argument("Maximum mark must be greater than zero");
    if (rawMark < 0.0 || rawMark > maximumMark)
        throw std::invalid_argument("Raw mark must be between zero and maximum mark");
    if (daysLate <= 0) return rawMark;
    if (daysLate > 3) return 0.0;
    return std::max(0.0, rawMark - maximumMark * 0.10 * daysLate);
}
