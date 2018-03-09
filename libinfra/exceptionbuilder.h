#ifndef EXCEPTIONBUILDER_H
#define EXCEPTIONBUILDER_H

#include <type_traits>
#include <memory>
#include <exception>
#include <QString>
#include <QDebug>

//
// A helper for building up a what()-string of a to-be-thrown exception.
//
// Use as follows:
//
//   throw static_cast<std::runtime_error>(ExceptionBuilder() << "This is a test:" << 1234);
//
class ExceptionBuilder
{
    // Store the string to-be-used as exception what()-string,
    // and a QDebug instance that operates on it.
    QString                  _exMsg;
    std::unique_ptr<QDebug>  _debug;

public:
    ExceptionBuilder() :
        _debug(std::make_unique<QDebug>(&_exMsg))
    {

    }

    // Forward all QDebug-supported output (I hope).
    template <typename T> ExceptionBuilder &operator<<(const T &arg)
    {
        if (!_debug) {
            _exMsg.append("...debug object gone");
            return *this;
        }
        *_debug << arg;
        return *this;
    }

    // Convert to exception object of choice.
    // For use, see the class comment, above.
    template <class E, typename = std::enable_if_t<std::is_base_of<std::exception, E>::value>>
    explicit operator E()
    {
        // Get rid of the QDebug instance so we can be sure the message
        // has been built up in the string.
        _debug.reset();

        // Convert to exception type E, and convert the built-up
        // exception message to std::string so E can use it.
        return E(_exMsg.toStdString());
    }
};

#endif // EXCEPTIONBUILDER_H
