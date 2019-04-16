#ifndef CONVERSIONSTORE_H
#define CONVERSIONSTORE_H

#include <QList>
#include <QSharedPointer>
#include <tuple>


template <typename Source, typename Result>
struct Upconvert
{
    using source_type = Source;
    using result_type = Result;

    const source_type  source;
    result_type        result;
    bool               success = false;
};


template <typename T> struct ConversionNode;

struct ConversionEdgeBase
{
    virtual ~ConversionEdgeBase() { }  // Ensure we have a vtable.
};

template <typename Source>
struct ConversionEdgeKnownSource : public ConversionEdgeBase
{
    using source_type = Source;
    using source_node_type = ConversionNode<source_type>;
    using source_node_ptr_type = QWeakPointer<source_node_type>;

    source_node_ptr_type  source_ptr;
};

template <typename Source, typename ...Result>
struct ConversionEdge : public ConversionEdgeKnownSource<Source>
{
    using results_nodes_ptrs_tuple_type
        = std::tuple<QSharedPointer<ConversionNode<Result>>...>;

    results_nodes_ptrs_tuple_type  results_ptrs;
};

template <typename Data>
struct ConversionNode
{
    using data_type = Data;

    QList<QSharedPointer<ConversionEdgeKnownSource<data_type>>>  edgesOut;
    QList<QWeakPointer<ConversionEdgeBase>>                      edgesIn;

    data_type  data;

    ConversionNode(const data_type &data) :
        data(data)
    {

    }
};

namespace detail {
template <typename EdgePtr, typename Result>
struct SetupEdgeIn
{
    EdgePtr *edge_ptr_ptr;

    SetupEdgeIn(EdgePtr *edge_ptr_ptr) : edge_ptr_ptr(edge_ptr_ptr) { }

    SetupEdgeIn &operator=(const QSharedPointer<ConversionNode<Result>> &oneResult) {
        oneResult->edgesIn.append(*edge_ptr_ptr);
        return *this;
    }
};

#if 0
template <typename T, typename U>
const T &constT(const T &t) { return t; }
#endif
}  // namespace detail

template <typename Source, typename ...Result>
QSharedPointer<ConversionEdge<Source, Result...>>
conversionNodeAddEdge(
    QSharedPointer<ConversionNode<Source>> source_ptr,
    std::tuple<QSharedPointer<ConversionNode<Result>>...> results_ptrs)
{
    auto edge_ptr = QSharedPointer<ConversionEdge<Source, Result...>>::create();
    edge_ptr->source_ptr = source_ptr;
    edge_ptr->results_ptrs = results_ptrs;

    source_ptr->edgesOut.append(edge_ptr);

    // Set up in-edges.
    //std::tuple<detail::SetupEdgeIn<decltype(edge_ptr), Result>...> setup(detail::constT<decltype(edge_ptr), Result>(edge_ptr)...);
    auto setup = std::make_tuple(detail::SetupEdgeIn<decltype(edge_ptr), Result>(&edge_ptr)...);
    setup = results_ptrs;

    return edge_ptr;
}


#endif // CONVERSIONSTORE_H
