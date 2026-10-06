#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMutex>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <iostream>

namespace Flux {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    VLC
};

class Logger : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList recentLogs READ recentLogs NOTIFY logsChanged)

public:
    static Logger& instance();

    void log(LogLevel level, const QString &category, const QString &message);
    QStringList recentLogs() const;
    QString logFilePath() const;

    Q_INVOKABLE void clear();

signals:
    void logsChanged();
    void logAdded(const QString &formattedLine);

private:
    Logger();
    ~Logger() override;

    QString levelToString(LogLevel level) const;

    mutable QMutex m_mutex;
    QStringList m_recentLogs;
    QFile m_logFile;
    QTextStream m_fileStream;
    const int m_maxRecentLogs = 200;
};

// Convenience logging macros
#define FLUX_LOG_INFO(cat, msg)  Flux::Logger::instance().log(Flux::LogLevel::Info, cat, msg)
#define FLUX_LOG_DEBUG(cat, msg) Flux::Logger::instance().log(Flux::LogLevel::Debug, cat, msg)
#define FLUX_LOG_WARN(cat, msg)  Flux::Logger::instance().log(Flux::LogLevel::Warning, cat, msg)
#define FLUX_LOG_ERROR(cat, msg) Flux::Logger::instance().log(Flux::LogLevel::Error, cat, msg)
#define FLUX_LOG_VLC(cat, msg)   Flux::Logger::instance().log(Flux::LogLevel::VLC, cat, msg)

} // namespace Flux
