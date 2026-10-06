#include "MediaLibrary.h"
#include "../core/Logger.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QUrl>

namespace Flux {

MediaLibraryModel::MediaLibraryModel(QObject *parent)
    : QAbstractListModel(parent) {
    loadDefault();
}

int MediaLibraryModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_roots.size());
}

QVariant MediaLibraryModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_roots.size())) {
        return QVariant();
    }

    const auto &root = m_roots[static_cast<size_t>(index.row())];

    switch (role) {
    case IdRole:           return root.id;
    case NameRole:         return root.name;
    case GroupRole:        return root.group;
    case UrlRole:          return root.url;
    case ServerOriginRole: return root.serverOrigin;
    case HrefRole:         return root.href;
    case DisplayNameRole:  return QString("[%1] %2").arg(root.group, root.name);
    case Qt::DisplayRole:  return root.name;
    default:               return QVariant();
    }
}

QHash<int, QByteArray> MediaLibraryModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole]           = "id";
    roles[NameRole]         = "name";
    roles[GroupRole]        = "group";
    roles[UrlRole]          = "url";
    roles[ServerOriginRole] = "serverOrigin";
    roles[HrefRole]         = "href";
    roles[DisplayNameRole]  = "displayName";
    return roles;
}

bool MediaLibraryModel::loadDefault() {
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        QDir(appDir).filePath("resources/media_library.json"),
        QDir(appDir).filePath("../../../resources/media_library.json"),
        QDir::current().filePath("resources/media_library.json"),
        ":/resources/media_library.json"
    };

    for (const auto &path : candidates) {
        if (QFile::exists(path)) {
            if (loadFromFile(path)) {
                return true;
            }
        }
    }

    FLUX_LOG_WARN("MediaLibrary", "media_library.json file not found in search paths, initializing hardcoded default library roots");

    // Fallback: Default 17 official library roots
    beginResetModel();
    m_roots = {
        {"english_movies", "English Movies", "Movies", "http://172.16.50.7/DHAKA-FLIX-7/English%20Movies/", "http://172.16.50.7", "/DHAKA-FLIX-7/English%20Movies/"},
        {"english_movies_1080p", "English Movies 1080p", "Movies", "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/", "http://172.16.50.14", "/DHAKA-FLIX-14/English%20Movies%20%281080p%29/"},
        {"hindi_movies", "Hindi Movies", "Movies", "http://172.16.50.14/DHAKA-FLIX-14/Hindi%20Movies/", "http://172.16.50.14", "/DHAKA-FLIX-14/Hindi%20Movies/"},
        {"south_movies", "South Indian Movies", "Movies", "http://172.16.50.14/DHAKA-FLIX-14/SOUTH%20INDIAN%20MOVIES/South%20Movies/", "http://172.16.50.14", "/DHAKA-FLIX-14/SOUTH%20INDIAN%20MOVIES/South%20Movies/"},
        {"south_movies_hindi_dubbed", "South-Movie Hindi Dubbed", "Movies", "http://172.16.50.14/DHAKA-FLIX-14/SOUTH%20INDIAN%20MOVIES/Hindi%20Dubbed/", "http://172.16.50.14", "/DHAKA-FLIX-14/SOUTH%20INDIAN%20MOVIES/Hindi%20Dubbed/"},
        {"foreign_movies", "Foreign Movies", "Movies", "http://172.16.50.7/DHAKA-FLIX-7/Foreign%20Language%20Movies/", "http://172.16.50.7", "/DHAKA-FLIX-7/Foreign%20Language%20Movies/"},
        {"kolkata_bangla_movies", "Kolkata Bangla Movies", "Movies", "http://172.16.50.7/DHAKA-FLIX-7/Kolkata%20Bangla%20Movies/", "http://172.16.50.7", "/DHAKA-FLIX-7/Kolkata%20Bangla%20Movies/"},
        {"satyajit_ray_films", "Satyajit Ray Films", "Movies", "http://172.16.50.7/DHAKA-FLIX-7/Kolkata%20Bangla%20Movies/Satyajit%20Ray%20Films/", "http://172.16.50.7", "/DHAKA-FLIX-7/Kolkata%20Bangla%20Movies/Satyajit%20Ray%20Films/"},
        {"animation_movies", "Animation Movies", "Movies", "http://172.16.50.14/DHAKA-FLIX-14/Animation%20Movies/", "http://172.16.50.14", "/DHAKA-FLIX-14/Animation%20Movies/"},
        {"animation_movies_1080p", "Animation Movies 1080p", "Movies", "http://172.16.50.14/DHAKA-FLIX-14/Animation%20Movies%20%281080p%29/", "http://172.16.50.14", "/DHAKA-FLIX-14/Animation%20Movies%20%281080p%29/"},
        {"movies_3d", "3D Movies", "Movies", "http://172.16.50.7/DHAKA-FLIX-7/3D%20Movies/", "http://172.16.50.7", "/DHAKA-FLIX-7/3D%20Movies/"},
        {"tv_web_series", "TV & Web Series", "Series", "http://172.16.50.12/DHAKA-FLIX-12/TV-WEB-Series/", "http://172.16.50.12", "/DHAKA-FLIX-12/TV-WEB-Series/"},
        {"korean_series", "Korean TV & Web Series", "Series", "http://172.16.50.14/DHAKA-FLIX-14/KOREAN%20TV%20%26%20WEB%20Series/", "http://172.16.50.14", "/DHAKA-FLIX-14/KOREAN%20TV%20%26%20WEB%20Series/"},
        {"cartoon_tv_series", "Cartoon TV Series", "Series", "http://172.16.50.10/DHAKA-FLIX-10/Anime%20%26%20Cartoon%20TV%20Series/", "http://172.16.50.10", "/DHAKA-FLIX-10/Anime%20%26%20Cartoon%20TV%20Series/"},
        {"documentary", "Documentary", "Other", "http://172.16.50.10/DHAKA-FLIX-10/Documentary/", "http://172.16.50.10", "/DHAKA-FLIX-10/Documentary/"},
        {"wwe_aew", "WWE & AEW Wrestling", "Other", "http://172.16.50.10/DHAKA-FLIX-10/WWE%20%26%20AEW%20Wrestling/", "http://172.16.50.10", "/DHAKA-FLIX-10/WWE%20%26%20AEW%20Wrestling/"},
        {"awards_tv_shows", "Awards & TV Shows", "Other", "http://172.16.50.10/DHAKA-FLIX-10/Awards%20%26%20TV%20Shows/", "http://172.16.50.10", "/DHAKA-FLIX-10/Awards%20%26%20TV%20Shows/"}
    };
    endResetModel();
    emit countChanged();
    return true;
}

bool MediaLibraryModel::loadFromFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        FLUX_LOG_ERROR("MediaLibrary", "Failed to open library configuration file: " + path);
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        FLUX_LOG_ERROR("MediaLibrary", "Failed to parse library configuration JSON: " + parseError.errorString());
        return false;
    }

    std::vector<MediaRoot> newRoots;
    QJsonArray array = doc.array();
    newRoots.reserve(static_cast<size_t>(array.size()));

    for (const auto &val : array) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        MediaRoot root;
        root.id = obj["id"].toString();
        root.name = obj["name"].toString();
        root.group = obj["group"].toString();
        root.url = obj["url"].toString();

        if (obj.contains("serverOrigin")) {
            root.serverOrigin = obj["serverOrigin"].toString();
        } else {
            QUrl qurl(root.url);
            root.serverOrigin = QString("%1://%2").arg(qurl.scheme(), qurl.host());
            if (qurl.port() != -1 && qurl.port() != 80) {
                root.serverOrigin += QString(":%1").arg(qurl.port());
            }
        }

        if (obj.contains("href")) {
            root.href = obj["href"].toString();
        } else {
            QUrl qurl(root.url);
            root.href = qurl.path();
        }

        if (!root.id.isEmpty() && !root.url.isEmpty()) {
            newRoots.push_back(root);
        }
    }

    FLUX_LOG_INFO("MediaLibrary", QString("Loaded %1 media library roots from %2").arg(newRoots.size()).arg(path));

    beginResetModel();
    m_roots = std::move(newRoots);
    endResetModel();
    emit countChanged();

    return true;
}

const MediaRoot* MediaLibraryModel::rootAt(int index) const {
    if (index >= 0 && index < static_cast<int>(m_roots.size())) {
        return &m_roots[static_cast<size_t>(index)];
    }
    return nullptr;
}

const MediaRoot* MediaLibraryModel::findById(const QString &id) const {
    for (const auto &root : m_roots) {
        if (root.id == id) {
            return &root;
        }
    }
    return nullptr;
}

QVariantMap MediaLibraryModel::getRoot(int index) const {
    QVariantMap map;
    const MediaRoot *r = rootAt(index);
    if (r) {
        map["id"]           = r->id;
        map["name"]         = r->name;
        map["group"]        = r->group;
        map["url"]          = r->url;
        map["serverOrigin"] = r->serverOrigin;
        map["href"]         = r->href;
        map["displayName"]  = QString("[%1] %2").arg(r->group, r->name);
    }
    return map;
}

int MediaLibraryModel::indexOfId(const QString &id) const {
    for (size_t i = 0; i < m_roots.size(); ++i) {
        if (m_roots[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace Flux
