#ifndef NUMERICRANGE_H
#define NUMERICRANGE_H

#include "numericconverter.h"
#include "exceptionbuilder.h"
#include "demangle.h"

#include <stdexcept>
#include <QString>
#include <QStringList>
#include <QList>

namespace HumanReadable {


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


    // Check for whether a value lies inside the range.

    int compare(I value) const
    {
        if (!hasLowerBound && !hasUpperBound) {
            // No bounds? Assume -infinity to +infinity => always within.
            // TODO: Or could, e.g. with floats, some value lie outside?
            return 0;
        }
        else if (!hasLowerBound) {
            if (value <= upperBoundValue)
                // Within.
                return 0;
            else
                // Greater than range.
                // TODO: Or could this be wrong for floats?
                return 1;
        }
        else if (!hasUpperBound) {
            if (lowerBoundValue <= value)
                // Within.
                return 0;
            else
                // Less than range.
                // TODO: Or could this be wrong for floats?
                return -1;
        }
        else {
            if (!(lowerBoundValue <= value))
                // Less than range.
                // TODO: floats?
                return -1;
            else if (!(value <= upperBoundValue))
                // Greater than range.
                // TODO: floats?
                return 1;
            else
                return 0;
        }
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

// Output to QDebug.
template <typename I, decltype(&numericConverter<I>) toI>
QDebug operator<<(QDebug debug, const NumericRange<I, toI> &range)
{
    QDebugStateSaver saver(debug);

    debug.nospace() << "NumericRange<" << DEMANGLE_TYPENAME(typeid(I).name()) << ">(";
    if (!range.hasLowerBound)
        debug       << "noLowerBound";
    else
        debug       << "lowerBound=" << range.lowerBoundValue;

    if (!range.hasUpperBound)
        debug       << " noUpperBound";
    else
        debug       << " upperBound=" << range.upperBoundValue;

    debug << ")";

    return debug;
}


// QList<NumericRange<...>> sub-class
template <typename I, decltype(&numericConverter<I>) toI = numericConverter<I>>
class NumericRangeList : public QList<NumericRange<I, toI>>
{
public:
    using base = QList<NumericRange<I, toI>>;
    using base::isEmpty;

    bool matches(I value)
    {
        if (isEmpty()) {
            // Matches due to no filter present.
            return true;
        }

        for (const auto &range : *this) {
            if (!range.compare(value) == 0)
                continue;

            // Matches due to that filter range.
            return true;
        }

        // No match.
        return false;
    }
};


}  // namespace HumanReadable

#endif // NUMERICRANGE_H
