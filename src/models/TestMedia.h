#pragma once

#include <QAbstractListModel>
#include <QString>
#include <vector>

namespace Flux {

struct TestMediaItem {
    QString title;
    QString year;
    QString badge;
    QString description;
    QString url;
};

class TestMediaModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        YearRole,
        BadgeRole,
        DescriptionRole,
        UrlRole
    };

    explicit TestMediaModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString getUrl(int index) const;

private:
    std::vector<TestMediaItem> m_items;
};

} // namespace Flux
