#include "TestMedia.h"

namespace Flux {

TestMediaModel::TestMediaModel(QObject *parent)
    : QAbstractListModel(parent) {

    m_items = {
        {
            "Ant-Man and the Wasp: Quantumania",
            "2023",
            "HEVC 10-bit • AAC 5.1",
            "1080p DSNP-WEB x265 HEVC 10bit AAC 5.1 MSubs-PSA",
            "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282023%29%201080p/Ant-Man%20and%20the%20Wasp-Quantumania%20%282023%29%201080p%20DSNP/Ant-Man%20and%20the%20Wasp%20Quantumania%20%282023%29%201080p%20DSNP-WEB%20x265%20HEVC%2010bit%20AAC%205.1%20MSubs-PSA.mkv"
        },
        {
            "Civil War",
            "2024",
            "Dual Audio: Hindi + English 5.1",
            "1080p BluRay x265 HEVC ESub [Dual Audio][Hindi 5.1+English 5.1] -mkvC",
            "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282024%29%201080p/Civil%20War%20%282024%29%201080p%20%5BDual%20Audio%5D/Civil%20War%20%282024%29%201080p%20BluRay%20x265%20HEVC%20ESub%20%5BDual%20Audio%5D%5BHindi%205.1%2BEnglish%205.1%5D%20-mkvC.mkv"
        },
        {
            "Kingdom S1E1",
            "2019",
            "Dual Audio: English + Korean 5.1",
            "Kingdom (TV Series 2019– ) S01E01 1080p NF WEBRip x265 HEVC MSubs [English 5.1+Korean 5.1] -OlaM",
            "http://172.16.50.14/DHAKA-FLIX-14/KOREAN%20TV%20%26%20WEB%20Series/Kingdom%20%28TV%20Series%202019%E2%80%93%20%29%201080p%20%5BDual%20Audio%5D/Season%201/Kingdom%20S01E01%20%201080p%20NF%20WEBRip%20x265%20HEVC%20MSubs%20%5BDual%20Audio%5D%5BEnglish%205.1%2BKorean%205.1%5D%20-OlaM.mkv"
        }
    };
}

int TestMediaModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_items.size());
}

QVariant TestMediaModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size())) {
        return QVariant();
    }

    const auto &item = m_items[static_cast<size_t>(index.row())];

    switch (role) {
    case TitleRole:       return item.title;
    case YearRole:        return item.year;
    case BadgeRole:       return item.badge;
    case DescriptionRole: return item.description;
    case UrlRole:         return item.url;
    default:              return QVariant();
    }
}

QHash<int, QByteArray> TestMediaModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[TitleRole] = "title";
    roles[YearRole] = "year";
    roles[BadgeRole] = "badge";
    roles[DescriptionRole] = "description";
    roles[UrlRole] = "url";
    return roles;
}

QString TestMediaModel::getUrl(int index) const {
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        return m_items[static_cast<size_t>(index)].url;
    }
    return QString();
}

} // namespace Flux
