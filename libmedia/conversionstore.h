#ifndef CONVERSIONSTORE_H
#define CONVERSIONSTORE_H

#include <QList>
#include <QMap>
#include <QSharedPointer>
#include <tuple>


template <typename T> struct ConversionNode;

struct ConversionEdgeBase
{
    using keyValueMetadata_type = QMap<QString, QString>;

    keyValueMetadata_type  keyValueMetadata;


    virtual ~ConversionEdgeBase() { }  // Ensure we have a vtable.


    bool matchesKeyValueMetadata(const keyValueMetadata_type &otherKVMetadata)
    {
        const keyValueMetadata_type::const_iterator iterEnd = otherKVMetadata.cend();
        for (keyValueMetadata_type::const_iterator iter = otherKVMetadata.cbegin();
             iter != iterEnd; ++iter)
        {
            const QString theirValue = iter.value();
            const QString ourValue = keyValueMetadata.value(iter.key());
            if (ourValue != theirValue)
                return false;
        }

        return true;
    }
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

    struct AncillaryDataBase
    {
        const QString  key;


        AncillaryDataBase(const QString &key) : key(key) { }

        virtual ~AncillaryDataBase() { }  // Ensure we have a vtable.
    };

    template <typename AData>
    struct AncillaryData : public AncillaryDataBase
    {
        using adata_type = AData;

        adata_type  adata;


        AncillaryData(const QString &key, const AData &adata) :
            AncillaryDataBase(key), adata(adata)
        {

        }
    };

    QMap<QString, QSharedPointer<AncillaryDataBase>>  adataMap;

    void addAdata(const QSharedPointer<AncillaryDataBase> &adata_ptr)
    {
        if (!adata_ptr)
            return;

        adataMap.insert(adata_ptr->key, adata_ptr);
    }

    template <typename AData>
    QSharedPointer<AncillaryData<AData>> addAdata(const QString &key, const AData &adata)
    {
        auto adata_ptr = QSharedPointer<AncillaryData<AData>>::create(key, adata);

        addAdata(adata_ptr);
        return adata_ptr;
    }


    ConversionNode() :
        data()
    {

    }

    ConversionNode(const data_type &data) :
        data(data)
    {

    }

    template <typename ...Args>
    ConversionNode(Args&&... args) :
        data(std::forward<Args>(args)...)
    {

    }


    template <typename ...Result>
    QList<QSharedPointer<ConversionEdge<data_type, Result...>>> findEdgesOutByResults(
        ConversionEdgeBase::keyValueMetadata_type edgeKeyValueMetadata = ConversionEdgeBase::keyValueMetadata_type()
        ) const
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

            if (!edgeResults_ptr->matchesKeyValueMetadata(edgeKeyValueMetadata))
                continue;

            ret.append(edgeResults_ptr);
        }

        return ret;
    }

    template <typename Source>
    QList<QSharedPointer<ConversionEdgeKnownSource<Source>>> findEdgesInBySource(
        ConversionEdgeBase::keyValueMetadata_type edgeKeyValueMetadata = ConversionEdgeBase::keyValueMetadata_type()
        ) const
    {
        QList<QSharedPointer<ConversionEdgeKnownSource<Source>>> ret;

        for (const QWeakPointer<ConversionEdgeBase> &edge_weak_ptr : edgesIn) {
            auto edge_ptr = edge_weak_ptr.toStrongRef();
            if (!edge_ptr)
                continue;

            auto edgeSource_ptr = edge_ptr.dynamicCast<ConversionEdgeKnownSource<Source>>();
            if (!edgeSource_ptr)
                continue;

            if (!edgeSource_ptr->matchesKeyValueMetadata(edgeKeyValueMetadata))
                continue;

            ret.append(edgeSource_ptr);
        }

        return ret;
    }

    template <typename Other>
    QList<QSharedPointer<ConversionNode<Other>>> findOtherFormat(
        ConversionEdgeBase::keyValueMetadata_type edgeKeyValueMetadata = ConversionEdgeBase::keyValueMetadata_type()
        ) const
    {
        QList<QSharedPointer<ConversionNode<Other>>> ret;

        const auto results = findEdgesOutByResults<Other>(edgeKeyValueMetadata);
        for (const QSharedPointer<ConversionEdge<data_type, Other>> &edge_ptr : results) {
            ret.append(std::get<0>(edge_ptr->results_ptrs));
        }

        const auto sources = findEdgesInBySource<Other>(edgeKeyValueMetadata);
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
    auto setup = std::make_tuple(detail::SetupEdgeIn<decltype(edge_ptr), Result>(&edge_ptr)...);
    setup = results_ptrs;

    return edge_ptr;
}


#endif // CONVERSIONSTORE_H
