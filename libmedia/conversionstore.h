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

    template <typename ...Result>
    QList<QSharedPointer<ConversionEdge<data_type, Result...>>> findEdgesOutByResults() const
    {
        QList<QSharedPointer<ConversionEdge<data_type, Result...>>> ret;

        for (const QSharedPointer<ConversionEdgeKnownSource<data_type>> &edge_ptr : edgesOut) {
            if (!edge_ptr)
                continue;

            // (The former gave a syntactic error on Debian 9 / GCC g++ 6.3 ...)
            //auto edgeResults_ptr = edge_ptr.dynamicCast<ConversionEdge<data_type, Result...>>();
            auto edgeResults_ptr = edge_ptr.template dynamicCast<ConversionEdge<data_type, Result...>>();
            if (!edgeResults_ptr)
                continue;

            ret.append(edgeResults_ptr);
        }

        return ret;
    }

    template <typename Source>
    QList<QSharedPointer<ConversionEdgeKnownSource<Source>>> findEdgesInBySource() const
    {
        QList<QSharedPointer<ConversionEdgeKnownSource<Source>>> ret;

        for (const QWeakPointer<ConversionEdgeBase> &edge_weak_ptr : edgesIn) {
            auto edge_ptr = edge_weak_ptr.toStrongRef();
            if (!edge_ptr)
                continue;

            auto edgeSource_ptr = edge_ptr.dynamicCast<ConversionEdgeKnownSource<Source>>();
            if (!edgeSource_ptr)
                continue;

            ret.append(edgeSource_ptr);
        }

        return ret;
    }

    template <typename Other>
    QList<QSharedPointer<ConversionNode<Other>>> findOtherFormat() const
    {
        QList<QSharedPointer<ConversionNode<Other>>> ret;

        const auto results = findEdgesOutByResults<Other>();
        for (const QSharedPointer<ConversionEdge<data_type, Other>> &edge_ptr : results) {
            ret.append(std::get<0>(edge_ptr->results_ptrs));
        }

        const auto sources = findEdgesInBySource<Other>();
        for (const QSharedPointer<ConversionEdgeKnownSource<Other>> &edge_ptr : sources) {
            ret.append(edge_ptr->source_ptr);
        }

        return ret;
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
