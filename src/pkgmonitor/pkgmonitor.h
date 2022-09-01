#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QSet>

class PkgMonitor : public QObject
{
    Q_OBJECT
public:
    explicit PkgMonitor(QObject *parent = nullptr);
    virtual ~PkgMonitor() override;

Q_SIGNALS:
    void pkgInstalled(const QString &pkgName);
    void pkgUpdated(const QString &pkgName);
    void pkgUninstalled(const QString &pkgName);

public Q_SLOTS:
    void onDPkgDirChanged(const QString &path);
    void onDPkgFileChanged(const QString &path);
    void onDPkgStatusFileChanged(const QString &path);

private:
    QString getPkgNameFromListFilePath(const QString &path) const;
    QSet<QString> getCurrentInstalledPkgNameSet() const;

private:
    QFileSystemWatcher *m_fileWatcher;
    QStringList m_monitoringFilePathList;

    QFileSystemWatcher *m_statusFileWatcher;
    QSet<QString> m_lastInstalledPkgNameSet;
};
