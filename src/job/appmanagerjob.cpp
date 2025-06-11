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
#include <QStandardPaths>

#include <zlib.h>
#include <aio.h> // async I/O

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

bool readKeyValueFile(QIODevice &device, QSettings::SettingsMap &map)
{
    QString content = device.readAll();

    QStringList lineStrList = content.split("\n");
    QString group;
    for (const QString &lineStr : lineStrList) {
        if (lineStr.startsWith("#")) {
            continue;
        }

        if (lineStr.startsWith("[") && lineStr.endsWith("]")) {
            group = lineStr.mid(1, lineStr.size() -2);
            continue;
        }

        QStringList keyValueStrList = lineStr.split("=");
        if (2 > keyValueStrList.size()) {
            continue;
        }

        QString key = keyValueStrList.first();
        keyValueStrList.removeFirst();
        QString value = keyValueStrList.join("=");
        map.insert(group + "/" + key, value);
    }
    return true;
}

bool writeKeyValueFile(QIODevice &device, const QSettings::SettingsMap &map)
{
    QStringList groupList;
    for (QSettings::SettingsMap::const_iterator cIter = map.begin();
         cIter != map.end(); ++cIter) {
        QString group = cIter.key().split("/").first();
        if (!groupList.contains(group)) {
            groupList.append(group);
        }
    }

    QStringList newLineStrList;
    for (const QString &group : groupList) {
        newLineStrList.append(QString("[%1]").arg(group));
        for (QSettings::SettingsMap::const_iterator cIter = map.begin();
             cIter != map.end(); ++cIter) {
            QString keyGroup = cIter.key().split("/").first();
            if (keyGroup != group) {
                continue;
            }
            QString key = cIter.key();
            key.remove(group + "/");

            QString newLineStr = key + "=" + cIter.value().toString();
            newLineStrList.append(newLineStr);
        }

        newLineStrList.append("");
    }

    device.write(newLineStrList.join("\n").toUtf8());
    return true;
}

const QSettings::Format ServiceSettingsFormat =
             QSettings::registerFormat("service", readKeyValueFile, writeKeyValueFile);

void setServiceSettingsValue(const QString &filePath, const QString &group, const QString &key, const QVariant &value)
{
    QSettings serviceSettings(filePath, ServiceSettingsFormat);
    serviceSettings.setIniCodec("utf-8");
    serviceSettings.beginGroup(group);
    serviceSettings.setValue(key, value);
    serviceSettings.endGroup();
    serviceSettings.sync();

    QFile f(filePath);
    if (!f.open(QIODevice::OpenModeFlag::ReadOnly)) {
        return;
    }

    QByteArray contentBa = f.readAll();
    f.close();

    if (!f.open(QIODevice::OpenModeFlag::WriteOnly)) {
       return;
    }
    f.write(contentBa);
    f.close();
    fsync(f.handle()); // 将文件数据同步到磁盘
}

QString getServiceSettingsValue(const QString &filePath, const QString &group, const QString &key)
{
    QSettings iniSettings(filePath, ServiceSettingsFormat);
    iniSettings.setIniCodec("utf-8");
    iniSettings.beginGroup(group);
    QString value = iniSettings.value(key).toString();
    iniSettings.endGroup();

    return value;
}

void setDesktopValue(const QString &desktopFilePath, const QString &key, const QVariant &value)
{
    setServiceSettingsValue(desktopFilePath, "Desktop Entry", key, value);
}

QString getDesktopValue(const QString &filePath, const QString &key)
{
    return getServiceSettingsValue(filePath, "Desktop Entry", key);
}

void setSystemdServiceValue(const QString &filePath, const QString &key, const QVariant &value)
{
    setServiceSettingsValue(filePath, "Service", key, value);
}

QString getSystemdServiceValue(const QString &filePath, const QString &key)
{
    return getServiceSettingsValue(filePath, "Service", key);
}

void setDBusServiceValue(const QString &filePath, const QString &key, const QVariant &value)
{
    setServiceSettingsValue(filePath, "D-BUS Service", key, value);
}

QString getDBusServiceValue(const QString &filePath, const QString &key)
{
    return getServiceSettingsValue(filePath, "D-BUS Service", key);
}

QString getDirKbSizeStrByCmd(const QString &dirPath)
{
    QProcess proc;
    proc.start("du", {"-s", dirPath});
    proc.waitForStarted();
    // 一直等到运行完成
    bool finished = false;
    while (!finished) {
        finished = proc.waitForFinished();
    }

    QString err = proc.readAllStandardError();
    if (!err.isEmpty()) {
        qCritical() << Q_FUNC_INFO << err;
    }

    QString ret = proc.readAllStandardOutput();
    proc.close();

    ret = ret.split("	").first();
    return ret;
}

AppManagerJob::AppManagerJob(QObject *parent)
    : QObject(parent)
    , m_runningStatus(Normal)
    , m_isOnlyLoadCurrentArchAppInfos(false)
    , m_isInitiallized(false)
    , m_downloadingFile(nullptr)
    , m_netManager(nullptr)
    , m_netReply(nullptr)
    , m_pkgMonitor(nullptr)
{
    m_currentCpuArchStr = QSysInfo::currentCpuArchitecture();
    m_currentCpuArchStr.replace("x86_64", "amd64");

    const QString &desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    m_downloadDirPath = QString("%1/downloadedPkg").arg(desktopPath);
    m_pkgBuildCacheDirPath = QString("/tmp/%1/%2/pkg-build-cache")
            .arg(MY_PKG_NAME)
            .arg(QDir::home().dirName());
    m_pkgBuildDirPath = QString("%1/pkgBuild").arg(desktopPath);
}

AppManagerJob::~AppManagerJob()
{
}

RunningStatus AppManagerJob::getRunningStatus()
{
    RunningStatus status;
    m_mutex.lock();
    status = m_runningStatus;
    m_mutex.unlock();

    return status;
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
    setRunningStatus(AM::Busy);
    m_mutex.lock(); // m_appInfosMap为成员变量，加锁
    m_appInfosMap.clear();
    m_mutex.unlock(); // 解锁

    reloadSourceUrlList();

    QDir aptPkgInfoListDir("/var/lib/apt/lists");
    const QStringList fileNameList = aptPkgInfoListDir.entryList(QDir::Filter::Files | QDir::Filter::NoDot | QDir::Filter::NoDotDot);
    // 判断架构包信息文件名过滤信息
    QString archPkgsFilterStr;
    if (m_isOnlyLoadCurrentArchAppInfos) {
        archPkgsFilterStr = QString("%1_Packages").arg(m_currentCpuArchStr);
    } else {
        archPkgsFilterStr = "_Packages";
    }

    for (const QString &fileName : fileNameList) {
        // 过滤出包信息文件路径
        if (fileName.endsWith(archPkgsFilterStr)) {
            const QString filePath = QString("%1/%2").arg(aptPkgInfoListDir.path()).arg(fileName); ////
            loadSrvAppInfosFromFile(m_appInfosMap, filePath);
        }
    }

    loadAllPkgInstalledAppInfos();

    Q_EMIT loadAppInfosFinished();

    setRunningStatus(AM::Normal);
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

void AppManagerJob::downloadPkgFile(const PkgInfo &info)
{
    m_downloadingPkgInfo = info;
    downloadFile(info.downloadUrl);
}

void AppManagerJob::downloadFile(const QString &url)
{
    if (!m_isInitiallized) {
        init();
    }

    QString realUrl = url;
    // 获取文件大小
    qint64 fileSize = getUrlFileSize(realUrl);
    qInfo() << Q_FUNC_INFO << fileSize;
    if (0 > fileSize) {
        Q_EMIT pkgFileDownloadFailed(m_downloadingPkgInfo);
        return;
    }
    qint64 endOffset = fileSize;

    // 创建下载路径
    QString fileName = QString(PKG_NAME_FORMAT_STR)
            .arg(m_downloadingPkgInfo.pkgName)
            .arg(m_downloadingPkgInfo.version)
            .arg(m_downloadingPkgInfo.arch);

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
    Q_EMIT pkgFileDownloadProgressChanged(m_downloadingPkgInfo, bytesRead, totalBytes);
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

    Q_EMIT pkgFileDownloadFinished(m_downloadingPkgInfo);
}

void AppManagerJob::startBuildPkgTask(const AppInfo &info, bool withDepends)
{
    bool successed = buildPkg(info.installedPkgInfo, withDepends);
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
    setRunningStatus(Busy);
    QString err;
    bool successed = installLocalPkg(OH_MY_DDE_LOCAL_PKG_PATH, err);
    Q_EMIT installOhMyDDEFinished(successed, err);
    setRunningStatus(Normal);
}

void AppManagerJob::holdPkgVersion(const QString &pkgName, bool hold)
{
    const QString &options = QString("echo %1 %2 | sudo dpkg --set-selections")
            .arg(pkgName).arg(hold ? "hold" : "install");

    QProcess proc;
    proc.start("pkexec", {"sh", "-c", options});
    proc.waitForStarted();
    proc.waitForFinished();

    QString err = proc.readAllStandardError();
    if (!err.isEmpty()) {
        qInfo() << Q_FUNC_INFO << err;
        proc.close();
        return;
    }

    proc.close();
    onPkgUpdated(pkgName);
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
        Q_EMIT appUpdated(m_appInfosMap.value(pkgName));
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

void AppManagerJob::setRunningStatus(RunningStatus status)
{
    if (m_runningStatus != status) {
        m_runningStatus = status;
        Q_EMIT runningStatusChanged(m_runningStatus);
    }
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
                pkgInfo.isInstalled = judgePkgIsInstalledFromStr(lineText);
                continue;
            }

            // 检测到下一包信息
            if (lineText.isEmpty()) {
                pkgInfo.infosFilePath = pkgInfosFilePath;
                pkgInfo.depositoryUrl = depositoryUrlStr;
                pkgInfo.contentOffset = lastPkgContentOffset;
                pkgInfo.contentSize = contentOffset - lastPkgContentOffset;
                lastPkgContentOffset = contentOffset;
                pkgInfoList.append(pkgInfo);
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
                pkgInfo.isInstalled = judgePkgIsInstalledFromStr(lineText);
                pkgInfo.isHoldVersion = lineText.contains("hold");
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
                pkgInfo.updatedTime = getPkgUpdatedTime(pkgInfo.pkgName, pkgInfo.arch);
                pkgInfoList.append(pkgInfo);
                pkgInfo = {};
            }
        }
    }
    pkgInfosFile.close();

    // 判断循环中最后一个包信息是否没添加到列表
    // 防止最后一个包信息的最后一行不是换行符，而忽略了该包信息
    if (!pkgInfo.pkgName.isEmpty()) {
        pkgInfoList.append(pkgInfo);
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
            pkgInfo.isInstalled = judgePkgIsInstalledFromStr(lineText);
            pkgInfo.isHoldVersion = lineText.contains("hold");
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
            pkgInfo.updatedTime = getPkgUpdatedTime(pkgInfo.pkgName, pkgInfo.arch);
            if (pkgName == pkgInfo.pkgName) {
                if (pkgInfo.isInstalled) {
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
        AppInfo *appInfo = &appInfosMap[pkgInfo.pkgName];
        appInfo->pkgName = pkgInfo.pkgName;
        appInfo->pkgInfoList.append(pkgInfo);
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
    // 优先为应用信息匹配当前架构已安装的包信息
    if (appInfo->installedPkgInfo.pkgName.isEmpty()
        || m_currentCpuArchStr == pkgInfo.arch) {
        appInfo->installedPkgInfo = pkgInfo;
    } else {
        m_mutex.unlock(); // 解锁
        return;
    }

    // 获取安装文件路径列表
    appInfo->installedPkgInfo.installedFileList = getAppInstalledFileList(appInfo->installedPkgInfo.pkgName, appInfo->installedPkgInfo.arch);
    // 获取desktop
    QStringList desktopPathList = getAppDesktopPathList(appInfo->installedPkgInfo.installedFileList,
                                                    appInfo->installedPkgInfo.pkgName);
    for (QStringList::const_iterator iter = desktopPathList.begin(); iter != desktopPathList.end(); ++iter) {
        appInfo->desktopInfo = getDesktopInfo(*iter);
        if (!appInfo->desktopInfo.desktopPath.isEmpty()) {
            break;
        }
    }

    m_mutex.unlock(); // 解锁
}

// 从包信息列表中加载已安装应用信息列表
void AppManagerJob::loadAllPkgInstalledAppInfos()
{
    QList<PkgInfo> pkgInfoList;
    getPkgInfoListFromFile(pkgInfoList, "/var/lib/dpkg/status");

    for (const PkgInfo &pkgInfo : pkgInfoList) {
        if (!pkgInfo.isInstalled) {
            continue;
        }
        loadPkgInstalledAppInfo(pkgInfo);
    }
}

QStringList AppManagerJob::getAppInstalledFileList(const QString &pkgName, const QString &arch)
{
    QStringList fileList;

    // 判断文件名中是否有架构名
    QString listFilePath = QString("/var/lib/dpkg/info/%1.list").arg(pkgName);
    QString archContent = QFile::exists(listFilePath) ? "" : QString(":%1").arg(arch);

    QFile installedListFile(QString("/var/lib/dpkg/info/%1%2.list").arg(pkgName).arg(archContent));
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

QStringList AppManagerJob::getAppDesktopPathList(const QStringList &list, const QString &pkgName)
{
    QStringList desktopPathList;
    for (const QString &path : list) {
        if (path.startsWith("/usr/share/applications/") && path.endsWith(".desktop")) {
            desktopPathList.append(path);
            continue;
        }

        if (path.startsWith(QString("/opt/apps/%1/entries/applications/").arg(pkgName)) && path.endsWith(".desktop")) {
            desktopPathList.append(path);
            continue;
        }
    }

    return desktopPathList;
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

QString AppManagerJob::getPkgUpdatedTime(const QString &pkgName, const QString &arch)
{
    // 判断文件名中是否有架构名
    QString listFilePath = QString("/var/lib/dpkg/info/%1.list").arg(pkgName);
    QString archContent = QFile::exists(listFilePath) ? "" : QString(":%1").arg(arch);

    QFileInfo fInfo(QString("/var/lib/dpkg/info/%1%2.list").arg(pkgName).arg(archContent));
    if (!fInfo.exists()) {
        qInfo() << Q_FUNC_INFO << fInfo.fileName() << "not exists!";
        return "";
    }

    return fInfo.lastModified().toString(DATE_TIME_FORMAT_STR);
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

bool AppManagerJob::buildPkg(const PkgInfo &pkgInfo, bool withDepends)
{
    if (pkgInfo.depends.isEmpty()) {
        qInfo() << Q_FUNC_INFO << pkgInfo.pkgName << "has no depends, use direct build method";
        withDepends = false;
    }

    //// 1. 清空打包缓存目录文件
    QDir pkgBuildCacheDir(m_pkgBuildCacheDirPath);
    if (pkgBuildCacheDir.exists()) {
        if (!pkgBuildCacheDir.removeRecursively()) {
            qCritical() << Q_FUNC_INFO << m_pkgBuildCacheDirPath << "removeRecursively failed!";
            return false;
        }
    }

    if (!pkgBuildCacheDir.mkpath(m_pkgBuildCacheDirPath)) {
        qCritical() << Q_FUNC_INFO << m_pkgBuildCacheDirPath << "create failed!";
        return false;
    }

    //// 2. 拷贝已安装的文件
    for (const QString &path : pkgInfo.installedFileList) {
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

    // 如果本次连同依赖一起打包
    if (withDepends) {
        //// 2.1 拷贝依赖库的文件
        const QString dependsDirPath = QString("/opt/apps/%1/files/%2-depends")
                .arg(pkgInfo.pkgName)
                .arg(pkgInfo.version);
        const QString newDependsDirPath = QString("%1%2")
                .arg(m_pkgBuildCacheDirPath)
                .arg(dependsDirPath);
        QStringList findedPkgNameList;
        QStringList dependPkgNameList = getPkgDepends(findedPkgNameList, pkgInfo.pkgName);
        for (const QString &dependPkgName : dependPkgNameList) {
            const PkgInfo &dependPkgInfo = m_appInfosMap.value(dependPkgName).installedPkgInfo;

            for (const QString &filePath : dependPkgInfo.installedFileList) {
                QFileInfo fileInfo(filePath);
                if (fileInfo.isDir()) {
                    continue;
                }

                if (filePath.startsWith("/usr/share/")) {
                    continue;
                }

                QString newFilePath = QString("%1%2")
                        .arg(newDependsDirPath)
                        .arg(filePath);
                QFileInfo newFileInfo(newFilePath);
                QString newFileDirPath = newFileInfo.dir().path();

                if (!newFileInfo.dir().exists()) {
                    bool ret = newFileInfo.dir().mkpath(newFileDirPath);
                    ret = 0;
                }

                QFile::copy(filePath, newFilePath);
            }
        }

        //// 2.2 修改desktop文件中启动命令，设置单独的依赖路径环境变量
        const QString execBashStrFormation = QString(
                    "#!/bin/sh\n"
                    "export LAUNCHER_DEPENDS_LOCATION=%1\n"
                    "export LD_LIBRARY_PATH=\"${LAUNCHER_DEPENDS_LOCATION}/lib:"
                    "${LAUNCHER_DEPENDS_LOCATION}/usr/bin:"
                    "${LAUNCHER_DEPENDS_LOCATION}/usr/lib:$LD_LIBRARY_PATH\"\n"
                    "\n"
                    "/sbin/ldconfig -p | grep -q libstdc++ || export LD_LIBRARY_PATH="
                    "\"$LD_LIBRARY_PATH:${LAUNCHER_DEPENDS_LOCATION}/libstdc++/\"\n"
                    "bash -c \"%2\" \"$@\"");

        for (const QString &path : pkgInfo.installedFileList) {
            QFileInfo fileInfo(path);
            if (fileInfo.isDir()) {
                continue;
            }

            bool isDesktop = path.endsWith(".desktop");
            bool isSystemdService = (path.startsWith("/lib/systemd/system/") && path.endsWith(".service"));
            bool isSysDBusService = (path.startsWith("/usr/share/dbus-1/system-services/") && path.endsWith(".service"));
            bool isSessionDBusService = (path.startsWith("/usr/share/dbus-1/services/") && path.endsWith(".service"));
            if (!isDesktop && !isSystemdService && !isSysDBusService && !isSessionDBusService) {
                continue;
            }

            // 生成执行shell脚本文件
            QString execBashFilePath = QString("/opt/apps/%1/files/%2-exec-bash/%3")
                    .arg(pkgInfo.pkgName)
                    .arg(pkgInfo.version)
                    .arg(fileInfo.fileName() + ".sh");
            QString newExecBashFilePath = QString("%1%2")
                    .arg(m_pkgBuildCacheDirPath)
                    .arg(execBashFilePath);

            // 检测待创建的执行shell脚本文件目录是否创建
            QFileInfo newExecBashFileInfo(newExecBashFilePath);
            QString newExecBashFileDirPath = newExecBashFileInfo.dir().path();
            if (!newExecBashFileInfo.dir().exists()) {
                newExecBashFileInfo.dir().mkpath(newExecBashFileDirPath);
            }

            QFile newExecBashFile(newExecBashFilePath);
            if (!newExecBashFile.open(QIODevice::OpenModeFlag::WriteOnly)) {
                qCritical() << Q_FUNC_INFO << newExecBashFilePath << "open failed!";
                return false;
            }

            // 获取desktop中执行命令和后缀
            QString newFilePath = m_pkgBuildCacheDirPath + path;
            QString execNoSuffix;
            QString execSuffix;
            if (isSystemdService) {
                execNoSuffix = getSystemdServiceValue(newFilePath, "ExecStart");
                execNoSuffix.remove("sudo ").remove("sudo,");
            } else if (isSysDBusService) {
                execNoSuffix = getDBusServiceValue(newFilePath, "Exec");
                execNoSuffix.remove("sudo ").remove("sudo,");
            } else if (isSessionDBusService) {
                execNoSuffix = getDBusServiceValue(newFilePath, "Exec");
            } else if (isDesktop) {
                execNoSuffix = getDesktopValue(newFilePath, "Exec");
                QStringList execList = execNoSuffix.split(" ");
                if (execList.last().startsWith("%")) {
                    execSuffix = execList.last();
                    execList.removeLast();
                    execNoSuffix = execList.join(" ");
                }
            }

            QByteArray content = execBashStrFormation.arg(dependsDirPath).arg(execNoSuffix).toUtf8();
            newExecBashFile.write(content);
            newExecBashFile.setPermissions(newExecBashFile.permissions()
                                            | QFile::Permission::ExeUser
                                            | QFile::Permission::ExeGroup
                                            | QFile::Permission::ExeOther
                                            | QFile::Permission::ExeOwner);
            newExecBashFile.close();
            fsync(newExecBashFile.handle());

            QString adjustExecPath = execBashFilePath;
            if (!execSuffix.isEmpty()) {
                adjustExecPath.append(" ");
                adjustExecPath.append(execSuffix);
            }

            if (isSystemdService) {
                setSystemdServiceValue(newFilePath, "ExecStart",  adjustExecPath);
            } else if (isSysDBusService) {
                setDBusServiceValue(newFilePath, "Exec", adjustExecPath);
            } else if (isSessionDBusService) {
                setDBusServiceValue(newFilePath, "Exec", adjustExecPath);
            } else if (isDesktop) {
                setDesktopValue(newFilePath, "Exec", adjustExecPath);
            }
        }
    }

    //// 3. 收集DEBIAN目录中文件
    QString outputDebianDir = QString("%1/DEBIAN").arg(m_pkgBuildCacheDirPath);
    QFileInfo outputDebianDirInfo(outputDebianDir);
    if (outputDebianDirInfo.dir().exists()) {
        outputDebianDirInfo.dir().mkdir(outputDebianDir);
    }
    // 收集DEBIAN/changelog文件
    QString changelogGZPath = QString("/usr/share/doc/%1/changelog.Debian.gz").arg(pkgInfo.pkgName);
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
    QString copyrightFilePath = QString("/usr/share/doc/%1/copyright").arg(pkgInfo.pkgName);
    QString copyrightFileOutputPath = QString("%1/copyright").arg(outputDebianDir);
    if (QFile::exists(copyrightFilePath)) {
        QFile::copy(copyrightFilePath, copyrightFileOutputPath);
    }

    // 判断文件名中是否有架构名
    QString listFilePath = QString("/var/lib/dpkg/info/%1.list").arg(pkgInfo.pkgName);
    QString archContent = QFile::exists(listFilePath) ? "" : QString(":%1").arg(pkgInfo.arch);

    // 收集DEBIAN/postinst文件
    QString postinstFilePath = QString("/var/lib/dpkg/info/%1%2.postinst").arg(pkgInfo.pkgName).arg(archContent);
    QString postinstFileOutputPath = QString("%1/postinst").arg(outputDebianDir);
    if (QFile::exists(postinstFilePath)) {
        QFile::copy(postinstFilePath, postinstFileOutputPath);
    }
    // 收集DEBIAN/postrm文件
    QString postrmFilePath = QString("/var/lib/dpkg/info/%1%2.postrm").arg(pkgInfo.pkgName).arg(archContent);
    QString postrmFileOutputPath = QString("%1/postrm").arg(outputDebianDir);
    if (QFile::exists(postrmFilePath)) {
        QFile::copy(postrmFilePath, postrmFileOutputPath);
    }
    // 收集DEBIAN/preinst文件
    QString preinstFilePath = QString("/var/lib/dpkg/info/%1%2.preinst").arg(pkgInfo.pkgName).arg(archContent);
    QString preinstFileOutputPath = QString("%1/preinst").arg(outputDebianDir);
    if (QFile::exists(preinstFilePath)) {
        QFile::copy(preinstFilePath, preinstFileOutputPath);
    }
    // 收集DEBIAN/prerm文件
    QString prermFilePath = QString("/var/lib/dpkg/info/%1%2.prerm").arg(pkgInfo.pkgName).arg(archContent);
    QString prermFileOutputPath = QString("%1/prerm").arg(outputDebianDir);
    if (QFile::exists(prermFilePath)) {
        QFile::copy(prermFilePath, prermFileOutputPath);
    }
    // 收集DEBIAN/conffiles文件
    QString conffilesFilePath = QString("/var/lib/dpkg/info/%1%2.conffiles").arg(pkgInfo.pkgName).arg(archContent);
    QString conffilesFileOutputPath = QString("%1/conffiles").arg(outputDebianDir);
    if (QFile::exists(conffilesFilePath)) {
        QFile::copy(conffilesFilePath, conffilesFileOutputPath);
    }
    // 收集DEBIAN/md5sums文件
    QString md5sumsFilePath = QString("/var/lib/dpkg/info/%1%2.md5sums").arg(pkgInfo.pkgName).arg(archContent);
    QString md5sumsFileOutputPath = QString("%1/md5sums").arg(outputDebianDir);
    if (QFile::exists(md5sumsFilePath)) {
        QFile::copy(md5sumsFilePath, md5sumsFileOutputPath);
    }
    // 收集DEBIAN/triggers文件
    QString triggersFilePath = QString("/var/lib/dpkg/info/%1%2.triggers").arg(pkgInfo.pkgName).arg(archContent);
    QString triggersFileOutputPath = QString("%1/triggers").arg(outputDebianDir);
    if (QFile::exists(triggersFilePath)) {
        QFile::copy(triggersFilePath, triggersFileOutputPath);
    }
    // 收集DEBIAN/shlibs文件
    QString shlibsFilePath = QString("/var/lib/dpkg/info/%1%2.shlibs").arg(pkgInfo.pkgName).arg(archContent);
    QString shlibsFileOutputPath = QString("%1/shlibs").arg(outputDebianDir);
    if (QFile::exists(shlibsFilePath)) {
        QFile::copy(shlibsFilePath, shlibsFileOutputPath);
    }
    // 收集DEBIAN/symbols文件
    QString symbolsFilePath = QString("/var/lib/dpkg/info/%1%2.symbols").arg(pkgInfo.pkgName).arg(archContent);
    QString symbolsFileOutputPath = QString("%1/symbols").arg(outputDebianDir);
    if (QFile::exists(symbolsFilePath)) {
        QFile::copy(symbolsFilePath, symbolsFileOutputPath);
    }

    //// 4. 收集DEBIAN/control文件
    // 读取包信息
    QString pkgControlInfos;
    QString pkgControlInfosHeader = QString("Package: %1").arg(pkgInfo.pkgName);
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

            // 如果本次连同依赖一起打包，则不需要Depends信息
            if (lineTxt.startsWith("Depends: ")) {
                if (withDepends) {
                    pkgControlInfos.append("Depends(origin): " + lineTxt);
                    pkgControlInfos.append("\n");
                    continue;
                }
            }

            // 计算占用大小
            if (lineTxt.startsWith("Installed-Size: ")) {
                pkgControlInfos.append("Installed-Size: " + getDirKbSizeStrByCmd(m_pkgBuildCacheDirPath));
                pkgControlInfos.append("\n");
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
                                   .arg(pkgInfo.pkgName)
                                   .arg(pkgInfo.version)
                                   .arg(pkgInfo.arch);
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

bool AppManagerJob::installLocalPkg(const QString &path, QString &err)
{
    QProcess *proc = new QProcess(this);
    proc->start("pkexec", {"dpkg", "-i", path});
    proc->waitForStarted();
    proc->write("Y\n");
    proc->waitForFinished();
    proc->waitForReadyRead();

    err = proc->readAllStandardError();
    if (!err.isEmpty()) {
        qInfo() << Q_FUNC_INFO << QString("install %1 failed! ").arg(path) << err;

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

QStringList AppManagerJob::getPkgDepends(QStringList &findedPkgNameList, const QString &pkgName)
{
//    qInfo() << Q_FUNC_INFO << "start" << pkgName;
    QStringList dependPkgNameList;

    if (IgnoredDependPkgNameListOfPkgBuildWithDepends.contains(pkgName)) {
        qInfo() << Q_FUNC_INFO << pkgName << "IgnoredDependPkgNameListOfPkgBuildWithDepends" << pkgName;
        return dependPkgNameList;
    }

    if (findedPkgNameList.contains(pkgName)) {
        qInfo() << Q_FUNC_INFO << pkgName << "has finded once!";
        return dependPkgNameList;
    }

    const PkgInfo installedPkgInfo = m_appInfosMap.value(pkgName).installedPkgInfo;
    if (installedPkgInfo.pkgName.isEmpty()) {
        qInfo() << Q_FUNC_INFO << pkgName << "is not installed!";
        return dependPkgNameList;
    }

    QString dependsStr = installedPkgInfo.depends;
//    qInfo() << Q_FUNC_INFO << pkgName << "dependsStr" << dependsStr;
    if (dependsStr.isEmpty()) {
        qInfo() << Q_FUNC_INFO << pkgName << "has no depends!";
        return dependPkgNameList;
    }

    QRegExp regExp;
    QStringList dependsStrFirstCutList = dependsStr.split(",");
    for (const QString &str : dependsStrFirstCutList) {
        QStringList dependsStrSecCutList = str.split("|");
        for (QString secStr : dependsStrSecCutList) {
            regExp.setPattern("\\(.*\\)");
            secStr.remove(regExp);

            regExp.setPattern(" +");
            secStr.remove(regExp);

            regExp.setPattern(":+");
            secStr.remove(regExp);

            findedPkgNameList.append(pkgName);
            QStringList childPkgDepends = getPkgDepends(findedPkgNameList, secStr);
            for (const QString &pkgName : childPkgDepends) {
                if (dependPkgNameList.contains(pkgName)) {
                    continue;
                }
                if (IgnoredDependPkgNameListOfPkgBuildWithDepends.contains(pkgName)) {
                    continue;
                }

                dependPkgNameList.append(pkgName);
            }
            if (dependPkgNameList.contains(secStr)) {
                continue;
            }
            dependPkgNameList.append(secStr);
        }
    }

    return dependPkgNameList;
}
