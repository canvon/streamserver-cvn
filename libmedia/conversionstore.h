#ifndef CONVERSIONSTORE_H
#define CONVERSIONSTORE_H

template <typename Source, typename Result>
struct Upconvert
{
    using source_type = Source;
    using result_type = Result;

    const source_type  source;
    result_type        result;
    bool               success = false;
};

#endif // CONVERSIONSTORE_H
