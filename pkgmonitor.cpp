#include "pkgmonitor.h"

#include <QDebug>
#include <QDir>

#define DPKG_INSTALL_APP_DIR_PATH "/var/lib/dpkg/info"
#define DPKG_INSTALL_STATUS_FILE_PATH "/var/lib/dpkg/status"

PkgMonitor::PkgMonitor(QObject *parent)
    : QObject(parent)
    , m_fileWatcher(nullptr)
    , m_statusFileWatcher(nullptr)

{
    // /var/lib/dpkg/info监视器
    m_fileWatcher = new QFileSystemWatcher(this);

    m_fileWatcher->addPath(DPKG_INSTALL_APP_DIR_PATH);

    // 添加文件路径监控
    QDir fileDir(DPKG_INSTALL_APP_DIR_PATH);
    QList<QFileInfo> fileInfo = fileDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files, QDir::DirsFirst);
    for (int i = 0; i < fileInfo.count(); ++i) {
        QString filePath = QDir::cleanPath(fileInfo.at(i).filePath());
        // 只监控desktop文件
        if (!filePath.endsWith(".list")) {
            continue;
        }

        QFileInfo fileInfo(filePath);
        if (0 == fileInfo.size()) {
            continue;
        }
        m_monitoringFilePathList.append(filePath);
        m_fileWatcher->addPath(filePath);
    }

    // /var/lib/dpkg/status监视器
    m_statusFileWatcher = new QFileSystemWatcher(this);
    m_statusFileWatcher->addPath(DPKG_INSTALL_STATUS_FILE_PATH);
    m_lastInstalledPkgNameSet = getCurrentInstalledPkgNameSet();

    // 初始化连接
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, &PkgMonitor::onDPkgDirChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &PkgMonitor::onDPkgFileChanged);

    connect(m_statusFileWatcher, &QFileSystemWatcher::fileChanged, this, &PkgMonitor::onDPkgStatusFileChanged);
}

PkgMonitor::~PkgMonitor()
{
}

void PkgMonitor::onDPkgDirChanged(const QString &path)
{
    qInfo() << Q_FUNC_INFO << path;

    QDir dir(path);
    QStringList currentFileList;

    QList<QFileInfo> fileInfo = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files, QDir::DirsFirst);
    for (int i = 0; i < fileInfo.count(); ++i) {
        QString filePath = QDir::cleanPath(fileInfo.at(i).filePath());
        // 只监控list文件
        if (!filePath.endsWith(".list")) {
            continue;
        }

        QFileInfo fileInfo(filePath);
        if (0 == fileInfo.size()) {
            continue;
        }
        currentFileList.append(filePath);
    }

    // 创建集合，用于比较
    QSet<QString> currentFileSet = QSet<QString>::fromList(currentFileList);
    QSet<QString> lastFileSet = QSet<QString>::fromList(m_monitoringFilePathList);

    // 找到新添加文件
    QStringList newFileList = (currentFileSet - lastFileSet).toList();

    // 找到已删除文件
    QStringList deletedFileList = (lastFileSet - currentFileSet).toList();

    if (!newFileList.isEmpty()) {
        qInfo() << Q_FUNC_INFO << "files added:" << newFileList;

        for (QStringList::const_iterator iter = newFileList.cbegin(); iter != newFileList.cend(); ++iter) {
             m_fileWatcher->addPath(*iter);
        }
    }

    if (!deletedFileList.isEmpty()) {
        qInfo() << Q_FUNC_INFO << "files deleted:" << deletedFileList;
        for (QStringList::const_iterator iter = deletedFileList.cbegin(); iter != deletedFileList.cend(); ++iter) {
            m_fileWatcher->removePath(*iter);
        }
    }

    // 更新监控的文件列表缓存
    m_monitoringFilePathList = currentFileList;
}

void PkgMonitor::onDPkgFileChanged(const QString &path)
{
    const QString pkgName = getPkgNameFromListFilePath(path);
    // 重新监控文件
    m_fileWatcher->removePath(path);
    m_fileWatcher->addPath(path);

    qInfo() << Q_FUNC_INFO << "pkgUpdated" << pkgName;
    Q_EMIT pkgUpdated(pkgName);
}

void PkgMonitor::onDPkgStatusFileChanged(const QString &path)
{
    qInfo() << Q_FUNC_INFO << path;
    const QSet<QString> &currentInstallPkgNameSet = getCurrentInstalledPkgNameSet();
    QSet<QString> newPkgNameSet = currentInstallPkgNameSet - m_lastInstalledPkgNameSet;
    QSet<QString> deletedPkgNameSet = m_lastInstalledPkgNameSet - currentInstallPkgNameSet;

    m_lastInstalledPkgNameSet = currentInstallPkgNameSet;

    if (!newPkgNameSet.isEmpty()) {
        qInfo() << Q_FUNC_INFO << "pkg installed:" << newPkgNameSet;

        for (QSet<QString>::const_iterator iter = newPkgNameSet.cbegin(); iter != newPkgNameSet.cend(); ++iter) {
             Q_EMIT pkgInstalled(*iter);
        }
    }

    if (!deletedPkgNameSet.isEmpty()) {
        qInfo() << Q_FUNC_INFO << "pkg uninstalled:" << deletedPkgNameSet;
        for (QSet<QString>::const_iterator iter = deletedPkgNameSet.cbegin(); iter != deletedPkgNameSet.cend(); ++iter) {
            Q_EMIT pkgUninstalled(*iter);
        }
    }

    // 重新监控文件
    m_statusFileWatcher->removePath(path);
    m_statusFileWatcher->addPath(path);
}

QString PkgMonitor::getPkgNameFromListFilePath(const QString &path) const
{
    // 获取包名,如/var/lib/dpkg/info/com.github.aaa.list -> com.github.aaa
    QString pkgName = path.split("/").last().remove(".list").split(":").first();
    return pkgName;
}

QSet<QString> PkgMonitor::getCurrentInstalledPkgNameSet() const
{
    QSet<QString> pkgNameSet;

    QFile pkgInfosFile(DPKG_INSTALL_STATUS_FILE_PATH);
    if (!pkgInfosFile.open(QIODevice::OpenModeFlag::ReadOnly)) {
        qDebug() << Q_FUNC_INFO << "open" << pkgInfosFile.fileName() << "failed!";
        return pkgNameSet;
    }

    QString pkgName;
    bool isInstalled = false;
    while (!pkgInfosFile.atEnd()) {
        const QByteArray &ba = pkgInfosFile.readLine();

        QString lineText = QString::fromUtf8(ba).remove("\n");
        if (lineText.startsWith("Package: ")) {
            pkgName = lineText.split(": ").last();
            continue;
        }

        if (lineText.startsWith("Status: ")) {
            isInstalled = lineText.contains("installed");
            continue;
        }

        // 检测到下一包信息
        if (lineText.isEmpty()) {
            if (isInstalled) {
                pkgNameSet.insert(pkgName);
            }
            pkgName.clear();
        }
    }

    if (!pkgName.isEmpty()) {
        if (isInstalled) {
            pkgNameSet.insert(pkgName);
        }
    }

    pkgInfosFile.close();
    return pkgNameSet;
}
