#include "MediaLibrary.h"
#include "../core/Logger.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QRegularExpression>
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
    return loadServer(1);
}

// ICC FTP: apply the user's session token and server address to the roots loaded from the
// library file / built-in list, so nothing about the server is fixed in the data.
void MediaLibraryModel::adjustIccRoots(const QString &session, const QString &baseUrl) {
    const QUrl base(baseUrl, QUrl::TolerantMode);
    const bool haveBase = base.isValid() && !base.host().isEmpty();
    const QString origin = haveBase ? (base.scheme() + QStringLiteral("://") + base.authority()) : QString();

    static const QRegularExpression reSession(QStringLiteral("session=[a-zA-Z0-9]+"));

    for (auto &root : m_roots) {
        if (!session.isEmpty() && root.url.contains(QLatin1String("session="))) {
            root.url.replace(reSession, QStringLiteral("session=") + session);
        }

        if (haveBase) {
            QUrl u(root.url, QUrl::TolerantMode);
            if (u.isValid() && !u.host().isEmpty()) {
                u.setScheme(base.scheme());
                u.setAuthority(base.authority());
                root.url = u.toString(QUrl::FullyEncoded);
            }
            root.serverOrigin = origin;
        }
    }
}

bool MediaLibraryModel::loadServer(int serverType, const QString &session, const QString &baseUrl) {
    m_serverType = (serverType == 2) ? 2 : ((serverType == 3) ? 3 : 1);
    QString filename;
    if (m_serverType == 3) {
        filename = QStringLiteral("resources/circle_library.json");
    } else if (m_serverType == 2) {
        filename = QStringLiteral("resources/icc_library.json");
    } else {
        filename = QStringLiteral("resources/media_library.json");
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        QDir(appDir).filePath(filename),
        QDir(appDir).filePath("../../../" + filename),
        QDir::current().filePath(filename),
        ":/" + filename
    };

    for (const auto &path : candidates) {
        if (QFile::exists(path)) {
            if (loadFromFile(path)) {
                if (m_serverType == 2) {
                    adjustIccRoots(session, baseUrl);
                    if (!m_roots.empty()) {
                        emit dataChanged(index(0), index(static_cast<int>(m_roots.size()) - 1));
                    }
                }
                return true;
            }
        }
    }

    FLUX_LOG_WARN("MediaLibrary", QString("Library json file (%1) not found, using hardcoded roots").arg(filename));

    beginResetModel();
    if (m_serverType == 3) {
        m_roots = {
            {"circle_eng_movies", "English Movies", "English Movies", "http://new.circleftp.net:5000/api/posts?categoryExact=6", "http://new.circleftp.net:5000", "/api/posts?categoryExact=6"},
            {"circle_featured", "Featured Post", "English Movies", "http://new.circleftp.net:5000/api/posts?categoryExact=80", "http://new.circleftp.net:5000", "/api/posts?categoryExact=80"},
            {"circle_hindi_movies", "Hindi Movies", "Hindi Movies", "http://new.circleftp.net:5000/api/posts?categoryExact=2", "http://new.circleftp.net:5000", "/api/posts?categoryExact=2"},
            {"circle_hindi_dubbed", "Hindi Dubbed Movies", "Hindi Movies", "http://new.circleftp.net:5000/api/posts?categoryExact=7", "http://new.circleftp.net:5000", "/api/posts?categoryExact=7"},
            {"circle_south_dubbed", "South Dubbed Movies", "Hindi Movies", "http://new.circleftp.net:5000/api/posts?categoryExact=3", "http://new.circleftp.net:5000", "/api/posts?categoryExact=3"},
            {"circle_anim_movies", "Animation Movies", "Animation", "http://new.circleftp.net:5000/api/posts?categoryExact=1", "http://new.circleftp.net:5000", "/api/posts?categoryExact=1"},
            {"circle_anim_dubbed", "Animation Dubbed", "Animation", "http://new.circleftp.net:5000/api/posts?categoryExact=20", "http://new.circleftp.net:5000", "/api/posts?categoryExact=20"},
            {"circle_anime_series", "Anime Series", "Animation", "http://new.circleftp.net:5000/api/posts?categoryExact=21", "http://new.circleftp.net:5000", "/api/posts?categoryExact=21"},
            {"circle_foreign_movies", "Foreign Movies", "Other Movies", "http://new.circleftp.net:5000/api/posts?categoryExact=8", "http://new.circleftp.net:5000", "/api/posts?categoryExact=8"},
            {"circle_south_movies", "South Indian Movies", "Other Movies", "http://new.circleftp.net:5000/api/posts?categoryExact=4", "http://new.circleftp.net:5000", "/api/posts?categoryExact=4"},
            {"circle_doc", "Documentary", "Other Movies", "http://new.circleftp.net:5000/api/posts?categoryExact=85", "http://new.circleftp.net:5000", "/api/posts?categoryExact=85"},
            {"circle_tv_eng", "English & Foreign TV Series", "TV & Web Series", "http://new.circleftp.net:5000/api/posts?categoryExact=9", "http://new.circleftp.net:5000", "/api/posts?categoryExact=9"},
            {"circle_tv_dubbed", "Dubbed TV Series", "TV & Web Series", "http://new.circleftp.net:5000/api/posts?categoryExact=22", "http://new.circleftp.net:5000", "/api/posts?categoryExact=22"},
            {"circle_tv_hindi", "Hindi TV Serials", "TV & Web Series", "http://new.circleftp.net:5000/api/posts?categoryExact=5", "http://new.circleftp.net:5000", "/api/posts?categoryExact=5"},
            {"circle_indian_shows", "Indian TV Shows", "TV & Web Series", "http://new.circleftp.net:5000/api/posts?categoryExact=238", "http://new.circleftp.net:5000", "/api/posts?categoryExact=238"},
            {"circle_wwe", "WWE", "TV & Web Series", "http://new.circleftp.net:5000/api/posts?categoryExact=15", "http://new.circleftp.net:5000", "/api/posts?categoryExact=15"},
            {"circle_awards", "Awards Shows", "TV & Web Series", "http://new.circleftp.net:5000/api/posts?categoryExact=251", "http://new.circleftp.net:5000", "/api/posts?categoryExact=251"},
            {"circle_software", "Software", "Software & Others", "http://new.circleftp.net:5000/api/posts?categoryExact=11", "http://new.circleftp.net:5000", "/api/posts?categoryExact=11"},
            {"circle_pc_games", "PC & Console Games", "Games & Apps", "http://new.circleftp.net:5000/api/posts?categoryExact=13", "http://new.circleftp.net:5000", "/api/posts?categoryExact=13"}
        };
    } else if (m_serverType == 2) {
        const QString s = session.isEmpty() ? QStringLiteral("b1bdfdae1f7e206b628274ef7eafadcd6094e07c46cd85b157c595174a369f96") : session;
        m_roots = {
            {"icc_eng_movies", "English Movies", "English Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=19", "http://10.16.100.244", "/dashboard.php?category=19"},
            {"icc_excl_1080p", "Exclusive Full-HD", "English Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=44", "http://10.16.100.244", "/dashboard.php?category=44"},
            {"icc_4k", "4K Movies", "English Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=74", "http://10.16.100.244", "/dashboard.php?category=74"},
            {"icc_3d", "3D Movies", "English Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=9", "http://10.16.100.244", "/dashboard.php?category=9"},
            {"icc_hindi", "Hindi Movies", "Hindi Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=2", "http://10.16.100.244", "/dashboard.php?category=2"},
            {"icc_south_dubbed", "South-Movie Hindi Dubbed", "Hindi Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=32", "http://10.16.100.244", "/dashboard.php?category=32"},
            {"icc_dual_audio", "Dual Audio", "Hindi Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=43", "http://10.16.100.244", "/dashboard.php?category=43"},
            {"icc_animated", "Animation Movies", "Animation", "http://10.16.100.244/dashboard.php?session=" + s + "&category=33", "http://10.16.100.244", "/dashboard.php?category=33"},
            {"icc_anime", "Anime Movies", "Animation", "http://10.16.100.244/dashboard.php?session=" + s + "&category=83", "http://10.16.100.244", "/dashboard.php?category=83"},
            {"icc_serials_anime", "Serials (Anime)", "Animation", "http://10.16.100.244/dashboard.php?session=" + s + "&category=78", "http://10.16.100.244", "/dashboard.php?category=78"},
            {"icc_foreign", "Other Foreign Movie", "Other Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=64", "http://10.16.100.244", "/dashboard.php?category=64"},
            {"icc_korean", "Korean Movies", "Other Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=75", "http://10.16.100.244", "/dashboard.php?category=75"},
            {"icc_documentary", "Documentaries", "Other Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=41", "http://10.16.100.244", "/dashboard.php?category=41"},
            {"icc_south", "South Indian Movies", "Regional Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=73", "http://10.16.100.244", "/dashboard.php?category=73"},
            {"icc_chinese", "Chinese Movies", "Regional Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=76", "http://10.16.100.244", "/dashboard.php?category=76"},
            {"icc_japanese", "Japanese Movies", "Regional Movies", "http://10.16.100.244/dashboard.php?session=" + s + "&category=80", "http://10.16.100.244", "/dashboard.php?category=80"},
            {"icc_series_eng", "TV & Web Series (Eng)", "TV & Web Series", "http://10.16.100.244/dashboard.php?session=" + s + "&category=36", "http://10.16.100.244", "/dashboard.php?category=36"},
            {"icc_series_hindi", "TV & Web Series (Hindi)", "TV & Web Series", "http://10.16.100.244/dashboard.php?session=" + s + "&category=37", "http://10.16.100.244", "/dashboard.php?category=37"},
            {"icc_series_dual", "TV Series (Dual Audio)", "TV & Web Series", "http://10.16.100.244/dashboard.php?session=" + s + "&category=72", "http://10.16.100.244", "/dashboard.php?category=72"},
            {"icc_wwe", "WWE Wrestling", "TV & Web Series", "http://10.16.100.244/dashboard.php?session=" + s + "&category=52", "http://10.16.100.244", "/dashboard.php?category=52"}
        };
    } else {
        m_roots = {
            {"english_movies", "English Movies", "English Movies", "http://172.16.50.7/DHAKA-FLIX-7/English%20Movies/", "http://172.16.50.7", "/DHAKA-FLIX-7/English%20Movies/"},
            {"english_movies_1080p", "English Movies 1080p", "English Movies", "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/", "http://172.16.50.14", "/DHAKA-FLIX-14/English%20Movies%20%281080p%29/"},
            {"awards_tv_shows", "Awards & TV Shows", "English Movies", "http://172.16.50.10/DHAKA-FLIX-10/Awards%20%26%20TV%20Shows/", "http://172.16.50.10", "/DHAKA-FLIX-10/Awards%20%26%20TV%20Shows/"},
            {"hindi_movies", "Hindi Movies", "Hindi Movies", "http://172.16.50.14/DHAKA-FLIX-14/Hindi%20Movies/", "http://172.16.50.14", "/DHAKA-FLIX-14/Hindi%20Movies/"},
            {"south_movies_hindi_dubbed", "South-Movie Hindi Dubbed", "Hindi Movies", "http://172.16.50.14/DHAKA-FLIX-14/SOUTH%20INDIAN%20MOVIES/Hindi%20Dubbed/", "http://172.16.50.14", "/DHAKA-FLIX-14/SOUTH%20INDIAN%20MOVIES/Hindi%20Dubbed/"},
            {"animation_movies", "Animation Movies", "Animation", "http://172.16.50.14/DHAKA-FLIX-14/Animation%20Movies/", "http://172.16.50.14", "/DHAKA-FLIX-14/Animation%20Movies/"},
            {"animation_movies_1080p", "Animation Movies 1080p", "Animation", "http://172.16.50.14/DHAKA-FLIX-14/Animation%20Movies%20%281080p%29/", "http://172.16.50.14", "/DHAKA-FLIX-14/Animation%20Movies%20%281080p%29/"},
            {"foreign_movies", "Foreign Movies", "Other Movies", "http://172.16.50.7/DHAKA-FLIX-7/Foreign%20Language%20Movies/", "http://172.16.50.7", "/DHAKA-FLIX-7/Foreign%20Language%20Movies/"},
            {"kolkata_bangla_movies", "Kolkata Bangla Movies", "Regional Movies", "http://172.16.50.7/DHAKA-FLIX-7/Kolkata%20Bangla%20Movies/", "http://172.16.50.7", "/DHAKA-FLIX-7/Kolkata%20Bangla%20Movies/"},
            {"satyajit_ray_films", "Satyajit Ray Films", "Regional Movies", "http://172.16.50.7/DHAKA-FLIX-7/Kolkata%20Bangla%20Movies/Satyajit%20Ray%20Films/", "http://172.16.50.7", "/DHAKA-FLIX-7/Kolkata%20Bangla%20Movies/Satyajit%20Ray%20Films/"},
            {"documentary", "Documentary", "Other Movies", "http://172.16.50.10/DHAKA-FLIX-10/Documentary/", "http://172.16.50.10", "/DHAKA-FLIX-10/Documentary/"},
            {"movies_3d", "3D Movies", "Other Movies", "http://172.16.50.7/DHAKA-FLIX-7/3D%20Movies/", "http://172.16.50.7", "/DHAKA-FLIX-7/3D%20Movies/"},
            {"south_movies", "South Indian Movies", "Regional Movies", "http://172.16.50.14/DHAKA-FLIX-14/SOUTH%20INDIAN%20MOVIES/South%20Movies/", "http://172.16.50.14", "/DHAKA-FLIX-14/SOUTH%20INDIAN%20MOVIES/South%20Movies/"},
            {"tv_web_series", "TV & Web Series", "TV & Web Series", "http://172.16.50.12/DHAKA-FLIX-12/TV-WEB-Series/", "http://172.16.50.12", "/DHAKA-FLIX-12/TV-WEB-Series/"},
            {"korean_series", "Korean TV & Web Series", "TV & Web Series", "http://172.16.50.14/DHAKA-FLIX-14/KOREAN%20TV%20%26%20WEB%20Series/", "http://172.16.50.14", "/DHAKA-FLIX-14/KOREAN%20TV%20%26%20WEB%20Series/"},
            {"cartoon_tv_series", "Cartoon TV Series", "TV & Web Series", "http://172.16.50.10/DHAKA-FLIX-10/Anime%20%26%20Cartoon%20TV%20Series/", "http://172.16.50.10", "/DHAKA-FLIX-10/Anime%20%26%20Cartoon%20TV%20Series/"},
            {"wwe_aew", "WWE & AEW Wrestling", "TV & Web Series", "http://172.16.50.10/DHAKA-FLIX-10/WWE%20%26%20AEW%20Wrestling/", "http://172.16.50.10", "/DHAKA-FLIX-10/WWE%20%26%20AEW%20Wrestling/"}
        };
    }
    if (m_serverType == 2) adjustIccRoots(session, baseUrl);
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
