#include "appmanagerjob.h"

#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QSettings>
#include <QTextCodec>
#include <QStandardItem>
#include <QMimeDatabase>

#include <zlib.h>
#include <aio.h> // async I/O

using namespace AM;

int zlibCompress(char *dest, int &destLen,
                 char *source, int sourceLen)
{
    unsigned char *dest1 = reinterpret_cast<unsigned char *>(dest);
    unsigned long destLen1 = static_cast<unsigned long>(destLen);
    unsigned char *source1 = reinterpret_cast<unsigned char *>(source);
    unsigned long sourceLen1 = static_cast<unsigned long>(sourceLen);

    return (compress(dest1, &destLen1, source1, sourceLen1));
}

int zlibUncompress(char *dest, int &destLen,
                   char *source, int sourceLen)
{
    unsigned char *dest1 = reinterpret_cast<unsigned char *>(dest);
    unsigned long destLen1 = static_cast<unsigned long>(destLen);
    unsigned char *source1 = reinterpret_cast<unsigned char *>(source);
    unsigned long sourceLen1 = static_cast<unsigned long>(sourceLen);

    return (uncompress(dest1, &destLen1, source1, sourceLen1));
}

enum ComPressError {
    Ok = 0,
    Fail = -1
};

ComPressError zlibUnCompress(const char *srcName, const char *destName)
{
    FILE *fp_out = nullptr;
    ComPressError re = ComPressError::Ok;

    gzFile in;
    int len = 0;
    char buf[16384];

    in = gzopen(srcName, "rb");

    if (in == nullptr) {
        return ComPressError::Fail;
    }

    if (nullptr == (fp_out = fopen(destName, "wb"))) {
        gzclose(in);
        sync();
        return ComPressError::Fail;
    }

    for (;;) {
        len = gzread(in, buf, sizeof(buf));

        if (len < 0) {
            re = ComPressError::Fail;
            break;
        }

        if (len == 0)
            break;

        unsigned long ulLen = static_cast<unsigned long>(len);
        if (fwrite(buf, 1, ulLen, fp_out) != ulLen) {
            re = ComPressError::Fail;
            break;
        }
    }

    fclose(fp_out);
    gzclose(in);
    sync();

    return re;
}

AppManagerJob::AppManagerJob(QObject *parent)
    : QObject(parent)
    , m_isInitiallized(false)
    , m_downloadingFile(nullptr)
    , m_netManager(nullptr)
    , m_netReply(nullptr)
    , m_listViewModel(nullptr)
    , m_pkgMonitor(nullptr)
{
    m_downloadDirPath = QString("%1/Desktop/downloadedPkg").arg(QDir::homePath());
    m_pkgBuildCacheDirPath = "/tmp/pkg-build-cache";
    m_pkgBuildDirPath = QString("%1/Desktop/pkgBuild").arg(QDir::homePath());
}

AppManagerJob::~AppManagerJob()
{
}

QMap<QString, AppInfo> AppManagerJob::getAppInfosMap()
{
    QMap<QString, AppInfo> appInfosMap;
    m_mutex.lock();
    appInfosMap = m_appInfosMap;
    m_mutex.unlock();

    return appInfosMap;
}

QList<AppInfo> AppManagerJob::getSearchedAppInfoList()
{
    QList<AppInfo> appInfoList;
    m_mutex.lock();
    appInfoList = m_searchedAppInfoList;
    m_mutex.unlock();

    return appInfoList;
}

QStandardItemModel *AppManagerJob::getListViewModel()
{
    QStandardItemModel *model = nullptr;
    m_mutex.lock();
    model = m_listViewModel;
    m_mutex.unlock();

    return model;
}

QList<AppInfo> AppManagerJob::getShowingAppInfoList()
{
    QList<AppInfo> appInfoList;
    m_mutex.lock();
    appInfoList = m_showingAppInfoList;
    m_mutex.unlock();

    return appInfoList;
}

QString AppManagerJob::getDownloadDirPath() const
{
    return m_downloadDirPath;
}

QString AppManagerJob::getPkgBuildDirPath() const
{
    return m_pkgBuildDirPath;
}

void AppManagerJob::init()
{
    reloadAppInfos();

    m_netManager = new QNetworkAccessManager(this);
    // 包监视器
    m_pkgMonitor = new PkgMonitor(this);

    initConnection();

    m_isInitiallized = true;
}

void AppManagerJob::reloadAppInfos()
{
    m_mutex.lock(); // m_appInfosMap为成员变量，加锁
    m_appInfosMap.clear();
    m_mutex.unlock(); // 解锁

    reloadSourceUrlList();

    QDir aptPkgInfoListDir("/var/lib/apt/lists");
    const QStringList fileNameList = aptPkgInfoListDir.entryList(QDir::Filter::Files | QDir::Filter::NoDot | QDir::Filter::NoDotDot);
    for (const QString &fileName : fileNameList) {
        if (fileName.endsWith("_Packages")) {
            const QString filePath = QString("%1/%2").arg(aptPkgInfoListDir.path()).arg(fileName); ////
            loadSrvAppInfosFromFile(m_appInfosMap, filePath);
        }
    }

    loadAllPkgInstalledAppInfos();

    Q_EMIT loadAppInfosFinished();
}

void AppManagerJob::downloadPkg(const QString &pkgName)
{
    QDir downloadDir(m_downloadDirPath);
    if (!downloadDir.exists()) {
        downloadDir.mkpath(m_downloadDirPath);
    }

    QProcess aptDownloadProc;
    QString cmd;
    cmd = QString("sh -c \"cd %1;apt-get download %2\"").arg(m_downloadDirPath).arg(pkgName);
    qInfo() << Q_FUNC_INFO << cmd;
    aptDownloadProc.start(cmd);
    aptDownloadProc.waitForStarted();
    aptDownloadProc.waitForFinished(-1);
    QString errorStr = aptDownloadProc.readAllStandardError();
    if (!errorStr.isEmpty()) {
        qInfo() << Q_FUNC_INFO << errorStr;
    }

    qInfo() << Q_FUNC_INFO << aptDownloadProc.readAllStandardOutput();
    Q_EMIT downloadPkgFinished(pkgName);
}

void AppManagerJob::downloadFile(const QString &url)
{
    if (!m_isInitiallized) {
        init();
    }

    m_downloadingFileOriginUrl = url;
    QString realUrl = url;
    // 获取文件大小
    qint64 fileSize = getUrlFileSize(realUrl);
    qInfo() << Q_FUNC_INFO << fileSize;
    qint64 endOffset = fileSize;

    // 创建下载路径
    QFileInfo info(realUrl);
    QString fileName = info.fileName();

    if (!fileName.contains(".")) {
        QFileInfo originUrlInfo(url);
        fileName = originUrlInfo.fileName();
    }

    if (fileName.isEmpty()) {
        fileName = "index.html";
    }

    QDir downloadDir(m_downloadDirPath);
    if (!downloadDir.exists()) {
        downloadDir.mkpath(m_downloadDirPath);
    }

    m_downloadingFile = new QFile(QString("%1/%2")
                                      .arg(m_downloadDirPath)
                                      .arg(fileName));
    if (!m_downloadingFile->open(QIODevice::OpenModeFlag::WriteOnly)) {
        qDebug() << Q_FUNC_INFO << m_downloadingFile->fileName() << "open failed";
        m_downloadingFile->deleteLater();
        m_downloadingFile = nullptr;
        return;
    }

    qInfo() << Q_FUNC_INFO << realUrl;
    QNetworkRequest request(realUrl);
    QString range = QString("bytes=%0-%1").arg(0).arg(endOffset);
    request.setRawHeader("Range", range.toUtf8());

    // https需要的配置（http不需要）
    QSslConfiguration sslConf = request.sslConfiguration();
    sslConf.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConf);

    m_netReply = m_netManager->get(request);
    connect(m_netReply, &QNetworkReply::readyRead, this, &AppManagerJob::onHttpReadyRead);
    connect(m_netReply, &QNetworkReply::downloadProgress, this, &AppManagerJob::onDownloadProgressChanged);
    connect(m_netReply, &QNetworkReply::finished, this, &AppManagerJob::onFileDownloadFinished);
}

void AppManagerJob::onHttpReadyRead()
{
    if (m_downloadingFile) {
        const QByteArray &ba = m_netReply->readAll();
        m_downloadingFile->write(ba);
    }
}

void AppManagerJob::onDownloadProgressChanged(qint64 bytesRead, qint64 totalBytes)
{
    Q_EMIT fileDownloadProgressChanged(m_downloadingFileOriginUrl, bytesRead, totalBytes);
}

void AppManagerJob::onFileDownloadFinished()
{
    m_netReply->deleteLater();
    m_netReply = nullptr;

    m_downloadingFile->flush();
    m_downloadingFile->close();
    sync();
    m_downloadingFile->deleteLater();
    m_downloadingFile = nullptr;

    Q_EMIT fileDownloadFinished(m_downloadingFileOriginUrl);
}

void AppManagerJob::startBuildPkgTask(const AppInfo &info)
{
    bool successed = buildPkg(info);
    Q_EMIT buildPkgTaskFinished(successed, info);
}

void AppManagerJob::startSearchTask(const QString &text)
{
    if (text.isEmpty()) {
        m_searchedAppInfoList = m_appInfosMap.values();
        Q_EMIT searchTaskFinished();
        return;
    }

    // 待匹配的字符串（不区分大小写）
    const QString matchingText = text.toLower();
    m_searchedAppInfoList.clear();
    for (const AppInfo &appInfo : m_appInfosMap.values()) {
        m_mutex.lock();
        // 匹配包名称
        // 不区分大小写
        const QString pkgNameLowerStr = appInfo.pkgName.toLower();
        if (pkgNameLowerStr.contains(matchingText)) {
            m_searchedAppInfoList.append(appInfo);
            m_mutex.unlock();
            continue;
        }

        // 应用名称对应的无声调拼音
        PinyinInfo pinYinInfo = AM::getPinYinInfoFromStr(appInfo.desktopInfo.appName);
        if (pinYinInfo.noTonePinYin.contains(matchingText)) {
            m_searchedAppInfoList.append(appInfo);
            m_mutex.unlock();
            continue;
        }
        // 应用名称拼音首字母缩写
        if (pinYinInfo.simpliyiedPinYin.contains(matchingText)) {
            m_searchedAppInfoList.append(appInfo);
            m_mutex.unlock();
            continue;
        }
        // 匹配应用名称
        // 不区分大小写
        const QString nameLowerStr = appInfo.desktopInfo.appName.toLower();
        if (nameLowerStr.contains(matchingText)) {
            m_searchedAppInfoList.append(appInfo);
            m_mutex.unlock();
            continue;
        }

        m_mutex.unlock();
    }
    Q_EMIT searchTaskFinished();
}

void AppManagerJob::createListViewMode(const QList<AppInfo> &list)
{
    QStandardItemModel *model = getItemModelFromAppInfoList(list);

    m_mutex.lock();
    m_showingAppInfoList = list;
    m_listViewModel = model;
    m_mutex.unlock();
    Q_EMIT createListViewModeFinished();
}

void AppManagerJob::uninstallPkg(const QString &pkgName)
{
    QProcess *proc = new QProcess(this);
    QString cmd = QString("pkexec apt-get remove %1").arg(pkgName);
    proc->start(cmd);
    proc->waitForStarted();
    proc->write("Y\n");
    proc->waitForFinished();
    proc->waitForReadyRead();

    QString error = proc->readAllStandardError();
    if (!error.isEmpty()) {
        qInfo() << Q_FUNC_INFO << QString("uninstall %1 failed! ").arg(pkgName) << error;

        proc->close();
        proc->deleteLater();
        proc = nullptr;
        return;
    }

    proc->close();
    proc->deleteLater();
    proc = nullptr;

    Q_EMIT uninstallPkgFinished(pkgName);
}

void AppManagerJob::installOhMyDDE()
{
    bool successed = installLocalPkg(OH_MY_DDE_LOCAL_PKG_PATH);
    Q_EMIT installOhMyDDEFinished(successed);
}

void AppManagerJob::installProcInfoPlugin()
{
    bool successed = installLocalPkg(PROC_INFO_PLUGIN_LOCAL_PKG_PATH);
    Q_EMIT installProcInfoPluginFinished(successed);
}

void AppManagerJob::onPkgInstalled(const QString &pkgName)
{
    PkgInfo pkgInfo;
    if (getInstalledPkgInfo(pkgInfo, pkgName)) {
        loadPkgInstalledAppInfo(pkgInfo);
        Q_EMIT appInstalled(m_appInfosMap.value(pkgName));
        qInfo() << Q_FUNC_INFO << pkgName;
    }
}

void AppManagerJob::onPkgUpdated(const QString &pkgName)
{
    PkgInfo pkgInfo;
    if (getInstalledPkgInfo(pkgInfo, pkgName)) {
        loadPkgInstalledAppInfo(pkgInfo);
        Q_EMIT appInstalled(m_appInfosMap.value(pkgName));
        qInfo() << Q_FUNC_INFO << pkgName;
    }
}

void AppManagerJob::onPkgUninstalled(const QString &pkgName)
{
    m_mutex.lock(); // m_appInfosMap为成员变量，加锁
    AppInfo *appInfo = &m_appInfosMap[pkgName];
    appInfo->isInstalled = false;
    appInfo->installedPkgInfo = {};
    appInfo->desktopInfo = {};
    m_mutex.unlock(); // 解锁

    PkgInfo pkgInfo;
    pkgInfo.pkgName = pkgName;
    Q_EMIT appUninstalled(*appInfo);
    qInfo() << Q_FUNC_INFO << pkgName;
}

void AppManagerJob::initConnection()
{
    connect(m_pkgMonitor, &PkgMonitor::pkgInstalled, this, &AppManagerJob::onPkgInstalled);
    connect(m_pkgMonitor, &PkgMonitor::pkgUpdated, this, &AppManagerJob::onPkgUpdated);
    connect(m_pkgMonitor, &PkgMonitor::pkgUninstalled, this, &AppManagerJob::onPkgUninstalled);
}

QList<QString> AppManagerJob::readSourceUrlList(const QString &filePath)
{
    QList<QString> sourceUrlList;

    QFile file(filePath);
    if (!file.open(QIODevice::OpenModeFlag::ReadOnly)) {
        qDebug() << Q_FUNC_INFO << file.fileName() << "open failed!";
        return sourceUrlList;
    }

    QTextStream txt(&file);
    while (!txt.atEnd()) {
        // 查找每行文字中的地址
        const QString &lineTxt = txt.readLine();
        if (lineTxt.startsWith("#")) {
            continue;
        }

        const QList<QString> &lineTxtPartList = lineTxt.split(" ");
        for (const QString &lineTxtPart : lineTxtPartList) {
            if (lineTxtPart.startsWith("http")) {
                sourceUrlList.append(lineTxtPart);
                break;
            }
        } // end - 查找每行文字中的地址
    }
    file.close();

    return sourceUrlList;
}

void AppManagerJob::reloadSourceUrlList()
{
    m_sourceUrlList.clear();
    m_sourceUrlList.append(readSourceUrlList("/etc/apt/sources.list"));

    const QString &sourceListDirPath = "/etc/apt/sources.list.d";
    QDir sourceListDir(sourceListDirPath);
    sourceListDir.setFilter(QDir::Filter::Files | QDir::Filter::NoDot | QDir::Filter::NoDotDot);
    for (const QString &sourceListFileName : sourceListDir.entryList()) {
        const QString &sourceListFilePath = QString("%1/%2").arg(sourceListDirPath).arg(sourceListFileName);
        qInfo() << Q_FUNC_INFO << sourceListFilePath;
        m_sourceUrlList.append(readSourceUrlList(sourceListFilePath));
    }
}

// 从包信息列表文件中获取应用信息列表
// isCompact : 是否获取简洁信息
bool AppManagerJob::getPkgInfoListFromFile(QList<PkgInfo> &pkgInfoList, const QString &pkgInfosFilePath, bool isCompact)
{
    // 从文件名中获取仓库网址，
    // 如：/var/lib/apt/lists/pools.uniontech.com_ppa_dde-eagle_dists_eagle_1041_main_binary-amd64_Packages
    QString depositoryUrlStr;
    QString depositoryUrlPartStr;
    // 先去掉末尾的"_Packages"字符
    if (pkgInfosFilePath.endsWith("_Packages")) {
        depositoryUrlPartStr = pkgInfosFilePath.left(pkgInfosFilePath.size() - QString("_Packages").size());
    }

    depositoryUrlPartStr = depositoryUrlPartStr.split("/").last().split("_dists").first().replace("_", "/");
    for (const QString &sourcesUrl : m_sourceUrlList) {
        if (sourcesUrl.contains(depositoryUrlPartStr)) {
            depositoryUrlStr = sourcesUrl;
            break;
        }
    }

    qInfo() << Q_FUNC_INFO << depositoryUrlStr;
    // 如果不是本地包信息列表文件，和没有找到仓库网址，则不解析
    if ("/var/lib/dpkg/status" != pkgInfosFilePath
        && depositoryUrlStr.isEmpty()) {
        return true;
    }

    QFile pkgInfosFile(pkgInfosFilePath);
    if (!pkgInfosFile.open(QIODevice::OpenModeFlag::ReadOnly)) {
        qDebug() << Q_FUNC_INFO << "open" << pkgInfosFile.fileName() << "failed!";
        return false;
    }

    PkgInfo pkgInfo;
    bool isInstalled = false;
    bool isReadingDescription = false;
    // 是否获取简洁信息
    if (isCompact) {
        qint64 lastPkgContentOffset = 0;
        qint64 contentOffset = 0;
        while (!pkgInfosFile.atEnd()) {
            const QByteArray ba = pkgInfosFile.readLine();
            contentOffset += ba.size();

            QString lineText = QString::fromUtf8(ba).remove("\n");
            if (lineText.startsWith("Package: ")) {
                pkgInfo.pkgName = lineText.split(": ").last();
                continue;
            }

            if (lineText.startsWith("Status: ")) {
                isInstalled = lineText.split(": ").last().startsWith("install");
                continue;
            }

            // 检测到下一包信息
            if (lineText.isEmpty()) {
                pkgInfo.infosFilePath = pkgInfosFilePath;
                pkgInfo.depositoryUrl = depositoryUrlStr;
                pkgInfo.contentOffset = contentOffset;
                pkgInfo.contentSize = contentOffset - lastPkgContentOffset;
                lastPkgContentOffset = contentOffset;
                if (isInstalled) {
                    pkgInfoList.append(pkgInfo);
                }
                pkgInfo = {};
            }
        }
    } else {
        while (!pkgInfosFile.atEnd()) {
            const QByteArray ba = pkgInfosFile.readLine();

            QString lineText = QString::fromUtf8(ba).remove("\n");
            if (lineText.startsWith("Package: ")) {
                pkgInfo.pkgName = lineText.split(": ").last();
                continue;
            }

            if (lineText.startsWith("Status: ")) {
                isInstalled = lineText.split(": ").last().startsWith("install");
                continue;
            }

            if (lineText.startsWith("Installed-Size: ")) {
                pkgInfo.installedSize = lineText.split(": ").last().toInt();
                continue;
            }
            if (lineText.startsWith("Maintainer: ")) {
                pkgInfo.maintainer = lineText.split(": ").last();
                continue;
            }
            if (lineText.startsWith("Architecture: ")) {
                pkgInfo.arch = lineText.split(": ").last();
                continue;
            }
            if (lineText.startsWith("Version: ")) {
                pkgInfo.version = lineText.split(": ").last();
                continue;
            }
            if (lineText.startsWith("Depends: ")) {
                pkgInfo.depends = lineText.split(": ").last();
                continue;
            }
            if (lineText.startsWith("Filename: ")) {
                const QString downloadFileName = lineText.split(": ").last();
                pkgInfo.downloadUrl = QString("%1/%2").arg(pkgInfo.depositoryUrl).arg(downloadFileName);
                continue;
            }
            if (lineText.startsWith("Size: ")) {
                pkgInfo.pkgSize = lineText.split(": ").last().toInt();
                continue;
            }

            if (lineText.startsWith("Homepage: ")) {
                pkgInfo.homepage = lineText.split(": ").last();
                continue;
            }


            if (lineText.startsWith("Description: ")) {
                pkgInfo.description = lineText.split(": ").last();
                pkgInfo.description.append("\n");
                isReadingDescription = true;
                continue;
            }
            if (lineText.startsWith(" ") && isReadingDescription) {
                pkgInfo.description += lineText;
                continue;
            }
            if (lineText.startsWith("Build-Depends: ")) {
                isReadingDescription = false;
                continue;
            }

            // 检测到下一包信息
            if (lineText.isEmpty()) {
                pkgInfo.infosFilePath = pkgInfosFilePath;
                pkgInfo.depositoryUrl = depositoryUrlStr;
                if (isInstalled) {
                    pkgInfoList.append(pkgInfo);
                }
                pkgInfo = {};
            }
        }
    }
    pkgInfosFile.close();

    // 判断循环中最后一个包信息是否没添加到列表
    // 防止最后一个包信息的最后一行不是换行符，而忽略了该包信息
    if (!pkgInfo.pkgName.isEmpty()) {
        if (isInstalled) {
            pkgInfoList.append(pkgInfo);
        }
    }

    qInfo() << Q_FUNC_INFO << "end";
    return true;
}

bool AppManagerJob::getInstalledPkgInfo(PkgInfo &pkgInfo, const QString &pkgName)
{
    const QString &localPkgInfosFilePath = "/var/lib/dpkg/status";
    QFile file(localPkgInfosFilePath);
    if (!file.open(QIODevice::OpenModeFlag::ReadOnly)) {
        qDebug() << Q_FUNC_INFO << "open" << file.fileName() << "failed!";
        return false;
    }

    bool isInstalled = false;
    bool isReadingDescription = false;
    while (!file.atEnd()) {
        const QByteArray ba = file.readLine();
        QString lineText = QString::fromUtf8(ba).remove("\n");

        // 不分析与目标包无关的信息
        if (!pkgInfo.pkgName.isEmpty() && pkgName != pkgInfo.pkgName) {
            if (lineText.isEmpty()) {
                pkgInfo.pkgName.clear();
            }
            continue;
        }

        if (lineText.startsWith("Package: ")) {
            pkgInfo.pkgName = lineText.split(": ").last();
            continue;
        }

        if (lineText.startsWith("Status: ")) {
            isInstalled = lineText.split(": ").last().startsWith("install");
            continue;
        }

        if (lineText.startsWith("Installed-Size: ")) {
            pkgInfo.installedSize = lineText.split(": ").last().toInt();
            continue;
        }
        if (lineText.startsWith("Maintainer: ")) {
            pkgInfo.maintainer = lineText.split(": ").last();
            continue;
        }
        if (lineText.startsWith("Architecture: ")) {
            pkgInfo.arch = lineText.split(": ").last();
            continue;
        }
        if (lineText.startsWith("Version: ")) {
            pkgInfo.version = lineText.split(": ").last();
            continue;
        }
        if (lineText.startsWith("Depends: ")) {
            pkgInfo.depends = lineText.split(": ").last();
            continue;
        }
        if (lineText.startsWith("Filename: ")) {
            const QString downloadFileName = lineText.split(": ").last();
            pkgInfo.downloadUrl = QString("%1/%2").arg(pkgInfo.depositoryUrl).arg(downloadFileName);
            continue;
        }
        if (lineText.startsWith("Size: ")) {
            pkgInfo.pkgSize = lineText.split(": ").last().toInt();
            continue;
        }

        if (lineText.startsWith("Homepage: ")) {
            pkgInfo.homepage = lineText.split(": ").last();
            continue;
        }

        if (lineText.startsWith("Description: ")) {
            pkgInfo.description = lineText.split(": ").last();
            pkgInfo.description.append("\n");
            isReadingDescription = true;
            continue;
        }
        if (lineText.startsWith(" ") && isReadingDescription) {
            pkgInfo.description += lineText;
            continue;
        }
        if (lineText.startsWith("Build-Depends: ")) {
            isReadingDescription = false;
            continue;
        }

        // 检测到下一包信息
        if (lineText.isEmpty()) {
            pkgInfo.infosFilePath = localPkgInfosFilePath;
            if (pkgName == pkgInfo.pkgName) {
                if (isInstalled) {
                    break;
                }
            }
            pkgInfo = {};
        }
    }
    file.close();

    // 判断循环中得到的最后一个包信息是否目标包信息
    return (pkgName == pkgInfo.pkgName);
}

// 从包信息列表中加载应用信息列表
void AppManagerJob::loadSrvAppInfosFromFile(QMap<QString, AppInfo> &appInfosMap, const QString &pkgInfosFilePath)
{
    QList<PkgInfo> pkgInfoList;
    getPkgInfoListFromFile(pkgInfoList, pkgInfosFilePath, true);
    qInfo() << Q_FUNC_INFO << pkgInfosFilePath << pkgInfoList.size();

    for (const PkgInfo &pkgInfo : pkgInfoList) {
        m_mutex.lock(); // appInfosMap为成员变量，加锁
        appInfosMap[pkgInfo.pkgName].pkgName = pkgInfo.pkgName;
        appInfosMap[pkgInfo.pkgName].pkgInfoList.append(pkgInfo);
        m_mutex.unlock(); // 解锁
    }
}

void AppManagerJob::loadPkgInstalledAppInfo(const AM::PkgInfo &pkgInfo)
{
    m_mutex.lock(); // m_appInfosMap为成员变量，加锁
    AppInfo *appInfo = &m_appInfosMap[pkgInfo.pkgName];
    if (appInfo->pkgName.isEmpty()) {
        appInfo->pkgName = pkgInfo.pkgName;
    }
    appInfo->isInstalled = true;
    appInfo->installedPkgInfo = pkgInfo;

    // 获取安装文件路径列表
    appInfo->installedPkgInfo.installedFileList = getAppInstalledFileList(appInfo->installedPkgInfo.pkgName);
    // 获取desktop
    appInfo->desktopInfo.desktopPath = getAppDesktopPath(appInfo->installedPkgInfo.installedFileList,
                                                         appInfo->installedPkgInfo.pkgName);
    appInfo->desktopInfo = getDesktopInfo(appInfo->desktopInfo.desktopPath);

    m_mutex.unlock(); // 解锁
}

// 从包信息列表中加载已安装应用信息列表
void AppManagerJob::loadAllPkgInstalledAppInfos()
{
    QList<PkgInfo> pkgInfoList;
    getPkgInfoListFromFile(pkgInfoList, "/var/lib/dpkg/status");

    for (const PkgInfo &pkgInfo : pkgInfoList) {
        loadPkgInstalledAppInfo(pkgInfo);
    }
}

QStringList AppManagerJob::getAppInstalledFileList(const QString &pkgName)
{
    QStringList fileList;

    QFile installedListFile(QString("/var/lib/dpkg/info/%1.list").arg(pkgName));
    if (!installedListFile.open(QIODevice::OpenModeFlag::ReadOnly)) {
        qInfo() << Q_FUNC_INFO << "open" << installedListFile.fileName() << "failed!";
        return fileList;
    }

    QTextStream txtStrem(&installedListFile);
    while (!txtStrem.atEnd()) {
        fileList.append(txtStrem.readLine());
    }
    installedListFile.close();

    return fileList;
}

QString AppManagerJob::getAppDesktopPath(const QStringList &list, const QString &pkgName)
{
    for (const QString &path : list) {
        if (path.startsWith("/usr/share/applications/") && path.endsWith(".desktop")) {
            return path;
        }

        if (path.startsWith(QString("/opt/apps/%1/entries/applications/").arg(pkgName)) && path.endsWith(".desktop")) {
            return path;
        }
    }

    return {};
}

DesktopInfo AppManagerJob::getDesktopInfo(const QString &desktop)
{
    DesktopInfo desktopInfo;

    QString desktopPath;
    QFileInfo fileInfo(desktop);
    if (fileInfo.isSymLink()) {
        desktopPath = fileInfo.readLink();
    } else {
        desktopPath = fileInfo.filePath();
    }

    // 是否为界面应用
    QSettings readIniSettingMethod(desktop, QSettings::Format::IniFormat);
    QTextCodec *textCodec = QTextCodec::codecForName("UTF-8");
    readIniSettingMethod.setIniCodec(textCodec);
    readIniSettingMethod.beginGroup("Desktop Entry");
    // 判断是否不显示
    bool noDisplay = readIniSettingMethod.value("NoDisplay").toBool();
    if (noDisplay) {
        return desktopInfo;
    }
    // "OnlyShowIn"属性为空或Deepin，则显示
    QString onlyShowIn = readIniSettingMethod.value("OnlyShowIn").toString();
    if (!onlyShowIn.isEmpty() && ONLY_SHOW_IN_VALUE_DEEPIN != onlyShowIn) {
        return desktopInfo;
    }
    // 桌面文件路径
    desktopInfo.desktopPath = desktopPath;
    // 应用名称
    QString sysLanguage = QLocale::system().name();
    QString sysLanguagePrefix = sysLanguage.split("_").first();
    QString xDeepinVendor = readIniSettingMethod.value("X-Deepin-Vendor").toString();
    if (X_DEEPIN_VENDOR_STR == xDeepinVendor) {
        desktopInfo.appName = readIniSettingMethod.value(QString("GenericName[%1]").arg(sysLanguage)).toString();
        // 如果没获取到语言对应的应用名称，则获取语言前缀对应的应用名称
        if (desktopInfo.appName.isEmpty()) {
            desktopInfo.appName = readIniSettingMethod.value(QString("GenericName[%1]").arg(sysLanguagePrefix)).toString();
        }
    } else {
        desktopInfo.appName = readIniSettingMethod.value(QString("Name[%1]").arg(sysLanguage)).toString();
        // 如果没获取到语言对应的应用名称，则获取语言前缀对应的应用名称
        if (desktopInfo.appName.isEmpty()) {
            desktopInfo.appName = readIniSettingMethod.value(QString("Name[%1]").arg(sysLanguagePrefix)).toString();
        }
    }

    if (desktopInfo.appName.isEmpty()) {
        desktopInfo.appName = readIniSettingMethod.value("Name").toString();
    }

    // 获取执行路径
    desktopInfo.exec = readIniSettingMethod.value("Exec").toString();
    // 获取执行路径
    desktopInfo.execPath = desktopInfo.exec.split(" ").first();
    // 判断是否是系统应用
    desktopInfo.isSysApp = true;
    if (desktopInfo.execPath.contains("/opt/"))
        desktopInfo.isSysApp = false;
    // 获取图标
    desktopInfo.themeIconName = readIniSettingMethod.value("Icon").toString();
    readIniSettingMethod.endGroup();

    return desktopInfo;
}

qint64 AppManagerJob::getUrlFileSize(QString &url, int tryTimes)
{
    qint64 size = -1;
    while (tryTimes--) {
        QNetworkAccessManager manager;
        QEventLoop loop;
        QNetworkRequest request;
        request.setUrl(url);

        QSslConfiguration sslConfig = request.sslConfiguration();
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
        request.setSslConfiguration(sslConfig);

        QNetworkReply *reply = manager.head(request);
        if (!reply) {
            continue;
        }
        connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            continue;
        }

        size = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        QVariant redirection = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (!redirection.isNull()) {
            url = redirection.toString();
            size = getUrlFileSize(url, tryTimes);
        }
        reply->deleteLater();
        break;
    }
    return size;
}

QStandardItemModel *AppManagerJob::getItemModelFromAppInfoList(const QList<AppInfo> &appInfoList)
{
    QStandardItemModel *model = new QStandardItemModel(this);
    for (const AppInfo &info : appInfoList) {
        QString appName = info.desktopInfo.appName;
        if (appName.isEmpty()) {
            appName = info.pkgName;
        }
        QStandardItem *item = new QStandardItem(appName);
        if (!info.desktopInfo.themeIconName.isEmpty()) {
            item->setIcon(QIcon::fromTheme(info.desktopInfo.themeIconName));
        } else {
            item->setIcon(QIcon::fromTheme(APP_THEME_ICON_DEFAULT));
        }

        item->setData(info.pkgName, AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME);
        model->appendRow(QList<QStandardItem *> {item});
    }

    return model;
}

bool AppManagerJob::buildPkg(const AppInfo &info)
{
    //// 1. 清空打包缓存目录文件
    QDir pkgBuildCacheDir(m_pkgBuildCacheDirPath);
    if (pkgBuildCacheDir.exists()) {
        if (!pkgBuildCacheDir.removeRecursively()) {
            return false;
        }
    }
    if (!pkgBuildCacheDir.mkdir(m_pkgBuildCacheDirPath)) {
        return false;
    }

    //// 2. 拷贝已安装的文件
    for (const QString &path : info.installedPkgInfo.installedFileList) {
        QFileInfo fileInfo(path);
        if (fileInfo.isDir()) {
            continue;
        }

        QString newFilePath = m_pkgBuildCacheDirPath + path;
        QFileInfo newFileInfo(newFilePath);
        QString newFileDirPath = newFileInfo.dir().path();

        if (!newFileInfo.dir().exists()) {
            bool ret = newFileInfo.dir().mkpath(newFileDirPath);
            ret = 0;
        }

        QFile::copy(path, newFilePath);
    }

    //// 3. 收集DEBIAN目录中文件
    QString outputDebianDir = QString("%1/DEBIAN").arg(m_pkgBuildCacheDirPath);
    QFileInfo outputDebianDirInfo(outputDebianDir);
    if (outputDebianDirInfo.dir().exists()) {
        outputDebianDirInfo.dir().mkdir(outputDebianDir);
    }
    // 收集DEBIAN/changelog文件
    QString changelogGZPath = QString("/usr/share/doc/%1/changelog.Debian.gz").arg(info.pkgName);
    QString changelogOutputPath = QString("%1/changelog").arg(outputDebianDir);
    // 先判断类型是否为gzip
    QMimeDatabase mimeDb;
    QMimeType mimeType;
    // 判断 changelog.Debian.gz 文件是否存在，如存在才判断类型
    QFileInfo changlogGZPathFileInfo(changelogGZPath);
    if (changlogGZPathFileInfo.exists()) {
        mimeType = mimeDb.mimeTypeForFile(changelogGZPath);
    }
    if (mimeType.name().startsWith("application/gzip")) {
        ComPressError er = zlibUnCompress(changelogGZPath.toStdString().data(), changelogOutputPath.toStdString().data());
        if (ComPressError::Ok != er) {
            qInfo() << changelogGZPath << "UnCompress error:" << er;
        }
    }

    // 收集DEBIAN/copyright文件
    QString copyrightFilePath = QString("/usr/share/doc/%1/copyright").arg(info.pkgName);
    QString copyrightFileOutputPath = QString("%1/copyright").arg(outputDebianDir);
    if (QFile::exists(copyrightFilePath)) {
        QFile::copy(copyrightFilePath, copyrightFileOutputPath);
    }

    // 收集DEBIAN/postinst文件
    QString postinstFilePath = QString("/var/lib/dpkg/info/%1.postinst").arg(info.pkgName);
    QString postinstFileOutputPath = QString("%1/postinst").arg(outputDebianDir);
    if (QFile::exists(postinstFilePath)) {
        QFile::copy(postinstFilePath, postinstFileOutputPath);
    }
    // 收集DEBIAN/postrm文件
    QString postrmFilePath = QString("/var/lib/dpkg/info/%1.postrm").arg(info.pkgName);
    QString postrmFileOutputPath = QString("%1/postrm").arg(outputDebianDir);
    if (QFile::exists(postrmFilePath)) {
        QFile::copy(postrmFilePath, postrmFileOutputPath);
    }
    // 收集DEBIAN/preinst文件
    QString preinstFilePath = QString("/var/lib/dpkg/info/%1.preinst").arg(info.pkgName);
    QString preinstFileOutputPath = QString("%1/preinst").arg(outputDebianDir);
    if (QFile::exists(preinstFilePath)) {
        QFile::copy(preinstFilePath, preinstFileOutputPath);
    }
    // 收集DEBIAN/prerm文件
    QString prermFilePath = QString("/var/lib/dpkg/info/%1.prerm").arg(info.pkgName);
    QString prermFileOutputPath = QString("%1/prerm").arg(outputDebianDir);
    if (QFile::exists(prermFilePath)) {
        QFile::copy(prermFilePath, prermFileOutputPath);
    }
    // 收集DEBIAN/conffiles文件
    QString conffilesFilePath = QString("/var/lib/dpkg/info/%1.conffiles").arg(info.pkgName);
    QString conffilesFileOutputPath = QString("%1/conffiles").arg(outputDebianDir);
    if (QFile::exists(conffilesFilePath)) {
        QFile::copy(conffilesFilePath, conffilesFileOutputPath);
    }
    // 收集DEBIAN/md5sums文件
    QString md5sumsFilePath = QString("/var/lib/dpkg/info/%1.md5sums").arg(info.pkgName);
    QString md5sumsFileOutputPath = QString("%1/md5sums").arg(outputDebianDir);
    if (QFile::exists(md5sumsFilePath)) {
        QFile::copy(md5sumsFilePath, md5sumsFileOutputPath);
    }
    // 收集DEBIAN/triggers文件
    QString triggersFilePath = QString("/var/lib/dpkg/info/%1.triggers").arg(info.pkgName);
    QString triggersFileOutputPath = QString("%1/triggers").arg(outputDebianDir);
    if (QFile::exists(triggersFilePath)) {
        QFile::copy(triggersFilePath, triggersFileOutputPath);
    }

    //// 4. 收集DEBIAN/control文件
    // 读取包信息
    QString pkgControlInfos;
    QString pkgControlInfosHeader = QString("Package: %1").arg(info.pkgName);
    bool isThisPkgInfo = false;
    QFile statusFile("/var/lib/dpkg/status");
    if (!statusFile.open(QIODevice::OpenModeFlag::ReadOnly)) {
        qInfo() << Q_FUNC_INFO << statusFile.fileName() << "open failed!";
        return false;
    }
    QTextStream txtStrem(&statusFile);
    while (!txtStrem.atEnd()) {
        QString lineTxt = txtStrem.readLine();
        // 检测到当前包信息
        if (lineTxt.startsWith(pkgControlInfosHeader)) {
            isThisPkgInfo = true;
        }
        // 检测到下一包信息
        if (lineTxt.isEmpty() && isThisPkgInfo) {
            isThisPkgInfo = false;
            break;
        }
        // 如果当前包信息，则添加到缓存字符串中
        if (isThisPkgInfo) {
            // 不需要Status信息
            if (lineTxt.startsWith("Status: ")) {
                continue;
            }

            pkgControlInfos.append(lineTxt);
            pkgControlInfos.append("\n");
        }
    }
    statusFile.close();
    sync();

    // 写DEBIAN/control文件内容
    QString controlFileOutputPath = QString("%1/control").arg(outputDebianDir);
    QFile controlFile(controlFileOutputPath);
    if (!controlFile.open(QIODevice::OpenModeFlag::WriteOnly | QIODevice::Text)) {
        qInfo() << Q_FUNC_INFO << controlFile.fileName() << "open failed!";
        return false;
    }
    controlFile.flush();
    QTextStream writeStrem(&controlFile);
    writeStrem << pkgControlInfos;
    controlFile.close();
    sync();

    //// 5. 打包
    QDir pkgBuildDir(m_pkgBuildDirPath);
    if (!pkgBuildDir.exists()) {
        pkgBuildDir.mkdir(m_pkgBuildDirPath);
    }
    QString pkgBuildFilePath = QString("%1/%2_%3_%4.deb")
                                   .arg(m_pkgBuildDirPath)
                                   .arg(info.pkgName)
                                   .arg(info.installedPkgInfo.version)
                                   .arg(info.installedPkgInfo.arch);
    QString cmd = QString("dpkg -b %1 %2").arg(m_pkgBuildCacheDirPath).arg(pkgBuildFilePath);
    QProcess dpkgBuildProc;
    dpkgBuildProc.start(cmd);
    dpkgBuildProc.waitForStarted();
    // 一直等到运行完成
    bool finished = false;
    while (!finished) {
        finished = dpkgBuildProc.waitForFinished();
    }

    QString buildErrorStr = dpkgBuildProc.readAllStandardError();
    if (!buildErrorStr.isEmpty()) {
        qInfo() << Q_FUNC_INFO << buildErrorStr;
        dpkgBuildProc.close();
        return false;
    }
    dpkgBuildProc.close();

    return true;
}

bool AppManagerJob::installLocalPkg(const QString &path)
{
    QProcess *proc = new QProcess(this);
    proc->start("pkexec", {"dpkg", "-i", path});
    proc->waitForStarted();
    proc->write("Y\n");
    proc->waitForFinished();
    proc->waitForReadyRead();

    QString error = proc->readAllStandardError();
    if (!error.isEmpty()) {
        qInfo() << Q_FUNC_INFO << QString("install %1 failed! ").arg(path) << error;

        proc->close();
        proc->deleteLater();
        proc = nullptr;
        return false;
    }

    proc->close();
    proc->deleteLater();
    proc = nullptr;

    return true;
}
