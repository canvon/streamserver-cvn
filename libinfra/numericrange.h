#ifndef NUMERICRANGE_H
#define NUMERICRANGE_H

#include "exceptionbuilder.h"

#include <stdexcept>
#include <QString>
#include <QStringList>

namespace HumanReadable {


template <typename I> I numericConverter(const QString &s, bool *ok = nullptr);

template <>
inline int numericConverter<int>(const QString &s, bool *ok) { return s.toInt(ok); }


template <typename I, I (*toI)(const QString &, bool *) = numericConverter<I>>
struct NumericRange
{
    bool  hasLowerBound = false;
    I     lowerBoundValue;
    bool  hasUpperBound = false;
    I     upperBoundValue;


    // Access with possibility of exception.

    I lowerBoundOrError() const
    {
        if (!hasLowerBound)
            throw std::runtime_error("Numeric range: Lower bound expected but there is none");

        return lowerBoundValue;
    }

    I upperBoundOrError() const
    {
        if (!hasUpperBound)
            throw std::runtime_error("Numeric range: Upper bound expected but there is none");

        return upperBoundValue;
    }


    // Access with hopefully-suitable replacement value.

    I lowerBoundOrTypeMin() const
    {
        if (!hasLowerBound) {
            using lim = std::numeric_limits<I>;
            if (lim::is_bounded)
                return lim::min();
            else
                return -lim::infinity();
        }

        return lowerBoundValue;
    }

    I upperBoundOrTypeMax() const
    {
        if (!hasUpperBound) {
            using lim = std::numeric_limits<I>;
            if (lim::is_bounded)
                return lim::max();
            else
                return lim::infinity();
        }

        return upperBoundValue;
    }


    // Setter

    void setLowerBound(I bound)
    {
        lowerBoundValue = bound;
        hasLowerBound = true;
    }

    void setUpperBound(I bound)
    {
        upperBoundValue = bound;
        hasUpperBound = true;
    }


    // Resetter

    void resetLowerBound()
    {
        hasLowerBound = false;
    }

    void resetUpperBound()
    {
        hasUpperBound = false;
    }


    // Conversion from string

    static NumericRange fromString(const QString &rangeStr)
    {
        NumericRange range;
        const QStringList rangeBounds = rangeStr.split('-');
        switch (rangeBounds.length()) {
        case 1:
        {
            const QString &bound(rangeBounds.first());
            if (bound.isEmpty()) {
                throw std::invalid_argument("Numeric range: Empty range");
            }
            bool ok = false;
            int boundNum = toI(bound, &ok);
            if (!ok) {
                throw static_cast<std::invalid_argument>(ExceptionBuilder()
                    << "Numeric range: Can't convert to number:" << bound);
            }

            range.setLowerBound(boundNum);
            range.setUpperBound(boundNum);
            break;
        }
        case 2:
        {
            const QString &from(rangeBounds.first());
            const QString &to(rangeBounds.last());
            if (from.isEmpty()) {
                range.resetLowerBound();
            }
            else {
                bool ok = false;
                int fromNum = toI(from, &ok);
                if (!ok) {
                    throw static_cast<std::invalid_argument>(ExceptionBuilder()
                        << "Numeric range: Can't convert lower bound to number:" << from);
                }
                range.setLowerBound(fromNum);
            }
            if (to.isEmpty()) {
                range.resetUpperBound();
            }
            else {
                bool ok = false;
                int toNum = toI(to, &ok);
                if (!ok) {
                    throw static_cast<std::invalid_argument>(ExceptionBuilder()
                        << "Numeric range: Can't convert upper bound to number:" << to);
                }
                range.setUpperBound(toNum);
            }
            break;
        }
        default:
            throw static_cast<std::invalid_argument>(ExceptionBuilder()
                << "Numeric range: Invalid range:" << rangeStr);
        }

        return range;
    }
};


}  // namespace HumanReadable

#endif // NUMERICRANGE_H
