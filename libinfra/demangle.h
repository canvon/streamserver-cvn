#ifndef DEMANGLE_H
#define DEMANGLE_H

#ifdef __GNUC__
// Added here to be able to manually demangle type name.
#include <cxxabi.h>
#define DEMANGLE_TYPENAME(mangled_name) \
    abi::__cxa_demangle((mangled_name), nullptr, nullptr, nullptr)
#else
#define DEMANGLE_TYPENAME(name) (name)
#endif

#endif // DEMANGLE_H
