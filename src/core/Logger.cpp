#include "Logger.h"
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>

namespace Flux {

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

Logger::Logger() {
    QString logDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(logDirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString logFilePath = dir.filePath("flux.log");
    m_logFile.setFileName(logFilePath);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_fileStream.setDevice(&m_logFile);
    }
}

Logger::~Logger() {
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen()) {
        m_fileStream.flush();
        m_logFile.close();
    }
}

QString Logger::levelToString(LogLevel level) const {
    switch (level) {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Info:    return "INFO ";
    case LogLevel::Warning: return "WARN ";
    case LogLevel::Error:   return "ERROR";
    case LogLevel::VLC:     return "VLC  ";
    }
    return "INFO ";
}

void Logger::log(LogLevel level, const QString &category, const QString &message) {
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString line = QString("[%1] [%2] [%3] %4")
                       .arg(timestamp)
                       .arg(levelToString(level))
                       .arg(category)
                       .arg(message);

    {
        QMutexLocker locker(&m_mutex);
        m_recentLogs.prepend(line);
        if (m_recentLogs.size() > m_maxRecentLogs) {
            m_recentLogs.removeLast();
        }

        if (m_logFile.isOpen()) {
            m_fileStream << line << "\n";
            m_fileStream.flush();
        }
    }

    std::cout << line.toStdString() << std::endl;

    emit logAdded(line);
    emit logsChanged();
}

QStringList Logger::recentLogs() const {
    QMutexLocker locker(&m_mutex);
    return m_recentLogs;
}

void Logger::clear() {
    QMutexLocker locker(&m_mutex);
    m_recentLogs.clear();
    emit logsChanged();
}

} // namespace Flux
