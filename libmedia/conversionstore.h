#ifndef CONVERSIONSTORE_H
#define CONVERSIONSTORE_H

#include <QList>
#include <QMap>
#include <QSharedPointer>
#include <tuple>


const static QString conversionSuccessKey = "success";


template <typename T> struct ConversionNode;

struct ConversionEdgeBase : public QEnableSharedFromThis<ConversionEdgeBase>
{
    using keyValueMetadata_type = QMap<QString, QString>;

    keyValueMetadata_type  keyValueMetadata;


    virtual ~ConversionEdgeBase() { }  // Ensure we have a vtable.


    virtual void clear() = 0;

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

    void mergeKeyValueMetadata(const keyValueMetadata_type &additionalKVMetadata)
    {
        const keyValueMetadata_type::const_iterator iterEnd = additionalKVMetadata.cend();
        for (keyValueMetadata_type::const_iterator iter = additionalKVMetadata.cbegin();
             iter != iterEnd; ++iter)
        {
            keyValueMetadata.insert(iter.key(), iter.value());
        }
    }

    bool wasSuccess() const
    {
        bool success = false, ok = false;
        int successInt = keyValueMetadata.value(conversionSuccessKey, "0").toInt(&ok);
        if (ok && successInt)
            success = true;
        return success;
    }

    void setSuccess(bool success)
    {
        keyValueMetadata.insert(conversionSuccessKey, QString::number(success));
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

template <typename Source, typename Result>
struct ConversionEdge : public ConversionEdgeKnownSource<Source>
{
    using result_type = Result;
    using result_node_type = ConversionNode<result_type>;
    using result_node_ptr_type = QSharedPointer<result_node_type>;

    using ConversionEdgeKnownSource<Source>::source_ptr;
    result_node_ptr_type  result_ptr;


    virtual void clear() override
    {
        // Delay our own destruction until leaving this function.
        auto keepAlive = this->sharedFromThis();

        do {
            auto source_strongPtr = source_ptr.toStrongRef();
            if (!source_strongPtr)
                break;

            auto &sourceEdgesOut(source_strongPtr->edgesOut);
            if (sourceEdgesOut.isEmpty())
                break;

            // Remove strong reference to this edge.
            for (int i = sourceEdgesOut.length() - 1; i >= 0; --i) {
                const auto &edge_ptr(sourceEdgesOut.at(i));
                if (edge_ptr.data() == this) {
                    sourceEdgesOut.removeAt(i);
                }
            }
        } while (false);

        // Clear pointer which is this edge's outgoing strong reference.
        result_ptr.clear();
    }
};

template <typename Data>
struct ConversionNode : public QEnableSharedFromThis<ConversionNode<Data>>
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


        AncillaryData(const QString &key, const adata_type &adata) :
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


    void clearEdges()
    {
        // Delay our own destruction until leaving this function.
        auto keepAlive = this->sharedFromThis();

        // Out-edges are weak pointers on the other side, so we can simply drop them.
        edgesOut.clear();

        // In-edges will be kept alive from the other side and would continue to point at us.
        if (!edgesIn.isEmpty()) {
            for (int i = edgesIn.length() - 1; i >= 0; --i) {
                auto edge_ptr = edgesIn.at(i).toStrongRef();
                if (edge_ptr)
                    edge_ptr->clear();
                edgesIn.removeAt(i);
            }
        }
    }

    template <typename Result>
    QList<QSharedPointer<ConversionEdge<data_type, Result>>> findEdgesOutByResults(
        ConversionEdgeBase::keyValueMetadata_type edgeKeyValueMetadata = ConversionEdgeBase::keyValueMetadata_type()
        ) const
    {
        QList<QSharedPointer<ConversionEdge<data_type, Result>>> ret;

        for (const QSharedPointer<ConversionEdgeKnownSource<data_type>> &edge_ptr : edgesOut) {
            if (!edge_ptr)
                continue;

            // (The former gave a syntactic error on Debian 9 / GCC g++ 6.3 ...)
            //auto edgeResults_ptr = edge_ptr.dynamicCast<ConversionEdge<data_type, Result>>();
            auto edgeResults_ptr = edge_ptr.template dynamicCast<ConversionEdge<data_type, Result>>();
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
    struct FindOtherFormatElement
    {
        QSharedPointer<ConversionNode<Other>>  node;
        bool                                   success;
    };

    template <typename Other>
    QList<FindOtherFormatElement<Other>> findOtherFormat(
        ConversionEdgeBase::keyValueMetadata_type edgeKeyValueMetadata = ConversionEdgeBase::keyValueMetadata_type()
        ) const
    {
        QList<FindOtherFormatElement<Other>> ret;

        const auto results = findEdgesOutByResults<Other>(edgeKeyValueMetadata);
        for (const QSharedPointer<ConversionEdge<data_type, Other>> &edge_ptr : results) {
            // Re-use success of stored conversion.
            ret.append({ edge_ptr->result_ptr, edge_ptr->wasSuccess() });
        }

        const auto sources = findEdgesInBySource<Other>(edgeKeyValueMetadata);
        for (const QSharedPointer<ConversionEdgeKnownSource<Other>> &edge_ptr : sources) {
            // Going back to original data is always a success.
            ret.append({ edge_ptr->source_ptr, true });
        }

        return ret;
    }
};

template <typename Source, typename Result>
QSharedPointer<ConversionEdge<Source, Result>>
conversionNodeAddEdge(
    QSharedPointer<ConversionNode<Source>> source_ptr,
    QSharedPointer<ConversionNode<Result>> result_ptr)
{
    auto edge_ptr = QSharedPointer<ConversionEdge<Source, Result>>::create();
    edge_ptr->source_ptr = source_ptr;
    edge_ptr->result_ptr = result_ptr;

    source_ptr->edgesOut.append(edge_ptr);
    result_ptr->edgesIn.append(edge_ptr);

    return edge_ptr;
}


#endif // CONVERSIONSTORE_H
