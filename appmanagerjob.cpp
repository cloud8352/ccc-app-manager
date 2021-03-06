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
    , m_listViewModel(nullptr)
{
    m_downloadDirPath = QString("%1/Desktop/dowonloadedPkg").arg(QDir::homePath());
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
    m_isInitiallized = true;
}

void AppManagerJob::reloadAppInfos()
{
    m_mutex.lock(); // m_appInfosMap????????????????????????
    m_appInfosMap.clear();
    m_mutex.unlock(); // ??????

    reloadSourceUrlList();

    m_appInfosMap.clear();
    QDir aptPkgInfoListDir("/var/lib/apt/lists");
    const QStringList fileNameList = aptPkgInfoListDir.entryList(QDir::Filter::Files | QDir::Filter::NoDot | QDir::Filter::NoDotDot);
    for (const QString &fileName : fileNameList) {
        if (fileName.endsWith("_Packages")) {
            const QString filePath = QString("%1/%2").arg(aptPkgInfoListDir.path()).arg(fileName); ////
            loadSrvAppInfosFromFile(m_appInfosMap, filePath);
        }
    }

    loadInstalledAppInfosFromFile(m_appInfosMap, "/var/lib/dpkg/status");

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
    // ??????????????????
    qint64 fileSize = getUrlFileSize(realUrl);
    qInfo() << Q_FUNC_INFO << fileSize;
    qint64 endOffset = fileSize;

    // ??????????????????
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

    // https??????????????????http????????????
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

    // ?????????????????????????????????????????????
    const QString matchingText = text.toLower();
    m_searchedAppInfoList.clear();
    for (const AppInfo &appInfo : m_appInfosMap.values()) {
        m_mutex.lock();
        // ???????????????
        // ??????????????????
        const QString pkgNameLowerStr = appInfo.pkgName.toLower();
        if (pkgNameLowerStr.contains(matchingText)) {
            m_searchedAppInfoList.append(appInfo);
            m_mutex.unlock();
            continue;
        }

        // ????????????????????????????????????
        PinyinInfo pinYinInfo = AM::getPinYinInfoFromStr(appInfo.desktopInfo.appName);
        if (pinYinInfo.noTonePinYin.contains(matchingText)) {
            m_searchedAppInfoList.append(appInfo);
            m_mutex.unlock();
            continue;
        }
        // ?????????????????????????????????
        if (pinYinInfo.simpliyiedPinYin.contains(matchingText)) {
            m_searchedAppInfoList.append(appInfo);
            m_mutex.unlock();
            continue;
        }
        // ??????????????????
        // ??????????????????
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
        qDebug() << Q_FUNC_INFO << QString("uninstall %1 failed! ").arg(pkgName) << error;
    }

    proc->close();
    proc->deleteLater();
    proc = nullptr;

    Q_EMIT uninstallPkgFinished(pkgName);
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
        // ??????????????????????????????
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
        } // end - ??????????????????????????????
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

// ???????????????????????????????????????????????????
bool AppManagerJob::getPkgInfoListFromFile(QList<PkgInfo> &pkgInfoList, const QString &pkgInfosFilePath)
{
    // ????????????????????????????????????
    // ??????/var/lib/apt/lists/pools.uniontech.com_ppa_dde-eagle_dists_eagle_1041_main_binary-amd64_Packages
    QString depositoryUrlStr;
    QString depositoryUrlPartStr;
    // ??????????????????"_Packages"??????
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
    // ????????????????????????????????????????????????????????????????????????????????????
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
    QTextStream txtStrem(&pkgInfosFile);
    while (!txtStrem.atEnd()) {
        QString lineTxt = txtStrem.readLine();
        if (lineTxt.startsWith("Package: ")) {
            pkgInfo.pkgName = lineTxt.split(": ").last();
            continue;
        }
        if (lineTxt.startsWith("Installed-Size: ")) {
            pkgInfo.installedSize = lineTxt.split(": ").last().toInt();
            continue;
        }
        if (lineTxt.startsWith("Maintainer: ")) {
            pkgInfo.maintainer = lineTxt.split(": ").last();
            continue;
        }
        if (lineTxt.startsWith("Architecture: ")) {
            pkgInfo.arch = lineTxt.split(": ").last();
            continue;
        }
        if (lineTxt.startsWith("Version: ")) {
            pkgInfo.version = lineTxt.split(": ").last();
            continue;
        }
        if (lineTxt.startsWith("Depends: ")) {
            pkgInfo.depends = lineTxt.split(": ").last();
            continue;
        }
        if (lineTxt.startsWith("Filename: ")) {
            const QString downloadFileName = lineTxt.split(": ").last();
            pkgInfo.downloadUrl = QString("%1/%2").arg(depositoryUrlStr).arg(downloadFileName);
            continue;
        }
        if (lineTxt.startsWith("Size: ")) {
            pkgInfo.pkgSize = lineTxt.split(": ").last().toInt();
            continue;
        }

        if (lineTxt.startsWith("Description: ")) {
            pkgInfo.description = lineTxt.split(": ").last();
            pkgInfo.description.append("\n");
            isReadingDescription = true;
            continue;
        }
        if (lineTxt.startsWith("Homepage: ")) {
            pkgInfo.homepage = lineTxt.split(": ").last();
            continue;
        }

        if (lineTxt.startsWith(" ") && isReadingDescription) {
            pkgInfo.description += lineTxt;
            continue;
        }

        // ????????????????????????
        if (lineTxt.isEmpty()) {
            pkgInfoList.append(pkgInfo);
            isReadingDescription = false;
        }
    }
    pkgInfosFile.close();

    qInfo() << Q_FUNC_INFO << "end";
    return true;
}

// ?????????????????????????????????????????????
void AppManagerJob::loadSrvAppInfosFromFile(QMap<QString, AppInfo> &appInfosMap, const QString &pkgInfosFilePath)
{
    QList<PkgInfo> pkgInfoList;
    getPkgInfoListFromFile(pkgInfoList, pkgInfosFilePath);
    qInfo() << Q_FUNC_INFO << pkgInfoList.size();

    for (const PkgInfo &pkgInfo : pkgInfoList) {
        m_mutex.lock(); // appInfosMap????????????????????????
        appInfosMap[pkgInfo.pkgName].pkgName = pkgInfo.pkgName;
        appInfosMap[pkgInfo.pkgName].pkgInfoList.append(pkgInfo);
        m_mutex.unlock(); // ??????
    }
}

// ??????????????????????????????????????????????????????
void AppManagerJob::loadInstalledAppInfosFromFile(QMap<QString, AppInfo> &appInfosMap, const QString &pkgInfosFilePath)
{
    QList<PkgInfo> pkgInfoList;
    getPkgInfoListFromFile(pkgInfoList, pkgInfosFilePath);

    for (const PkgInfo &pkgInfo : pkgInfoList) {
        m_mutex.lock(); // appInfosMap????????????????????????
        AppInfo *appInfo = &appInfosMap[pkgInfo.pkgName];
        appInfo->pkgName = pkgInfo.pkgName;
        appInfo->isInstalled = true;
        appInfo->installedPkgInfo = pkgInfo;
        // ???????????????????????????????????????????????????????????????
        for (const PkgInfo &srvPkgInfo : appInfo->pkgInfoList) {
            if (appInfo->installedPkgInfo.version == srvPkgInfo.version) {
                appInfo->installedPkgInfo.pkgSize = srvPkgInfo.pkgSize;
                appInfo->installedPkgInfo.downloadUrl = srvPkgInfo.downloadUrl;
                break;
            }
        }

        // ??????????????????????????????
        appInfo->installedPkgInfo.installedFileList = getAppInstalledFileList(appInfo->installedPkgInfo.pkgName);
        // ??????desktop
        appInfo->desktopInfo.desktopPath = getAppDesktopPath(appInfo->installedPkgInfo.installedFileList,
                                                             appInfo->installedPkgInfo.pkgName);
        appInfo->desktopInfo = getDesktopInfo(appInfo->desktopInfo.desktopPath);
        m_mutex.unlock(); // ??????
    }
}

QStringList AppManagerJob::getAppInstalledFileList(const QString &pkgName)
{
    QStringList fileList;

    QFile installedListFile(QString("/var/lib/dpkg/info/%1.list").arg(pkgName));
    if (!installedListFile.open(QIODevice::OpenModeFlag::ReadOnly)) {
        qDebug() << Q_FUNC_INFO << "open" << installedListFile.fileName() << "failed!";
        return fileList;
    }

    QTextStream txtStrem(&installedListFile);
    while (!txtStrem.atEnd()) {
        fileList.append(txtStrem.readLine());
    }
    installedListFile.close();
    sync();

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

    // ?????????????????????
    QSettings readIniSettingMethod(desktop, QSettings::Format::IniFormat);
    QTextCodec *textCodec = QTextCodec::codecForName("UTF-8");
    readIniSettingMethod.setIniCodec(textCodec);
    readIniSettingMethod.beginGroup("Desktop Entry");
    // ?????????????????????
    bool noDisplay = readIniSettingMethod.value("NoDisplay").toBool();
    if (noDisplay) {
        return desktopInfo;
    }
    // "OnlyShowIn"???????????????Deepin????????????
    QString onlyShowIn = readIniSettingMethod.value("OnlyShowIn").toString();
    if (!onlyShowIn.isEmpty() && ONLY_SHOW_IN_VALUE_DEEPIN != onlyShowIn) {
        return desktopInfo;
    }
    // ??????????????????
    desktopInfo.desktopPath = desktopPath;
    // ????????????
    QString sysLanguage = QLocale::system().name();
    QString sysLanguagePrefix = sysLanguage.split("_").first();
    QString xDeepinVendor = readIniSettingMethod.value("X-Deepin-Vendor").toString();
    if (X_DEEPIN_VENDOR_STR == xDeepinVendor) {
        desktopInfo.appName = readIniSettingMethod.value(QString("GenericName[%1]").arg(sysLanguage)).toString();
        // ??????????????????????????????????????????????????????????????????????????????????????????
        if (desktopInfo.appName.isEmpty()) {
            desktopInfo.appName = readIniSettingMethod.value(QString("GenericName[%1]").arg(sysLanguagePrefix)).toString();
        }
    } else {
        desktopInfo.appName = readIniSettingMethod.value(QString("Name[%1]").arg(sysLanguage)).toString();
        // ??????????????????????????????????????????????????????????????????????????????????????????
        if (desktopInfo.appName.isEmpty()) {
            desktopInfo.appName = readIniSettingMethod.value(QString("Name[%1]").arg(sysLanguagePrefix)).toString();
        }
    }

    if (desktopInfo.appName.isEmpty()) {
        desktopInfo.appName = readIniSettingMethod.value("Name").toString();
    }

    // ??????????????????
    desktopInfo.exec = readIniSettingMethod.value("Exec").toString();
    // ??????????????????
    desktopInfo.execPath = desktopInfo.exec.split(" ").first();
    // ???????????????????????????
    desktopInfo.isSysApp = true;
    if (desktopInfo.execPath.contains("/opt/"))
        desktopInfo.isSysApp = false;
    // ????????????
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
        model->appendRow(QList<QStandardItem *> {item});
    }

    return model;
}

bool AppManagerJob::buildPkg(const AppInfo &info)
{
    //// 1. ??????????????????????????????
    QDir pkgBuildCacheDir(m_pkgBuildCacheDirPath);
    if (pkgBuildCacheDir.exists()) {
        if (!pkgBuildCacheDir.removeRecursively()) {
            return false;
        }
    }
    if (!pkgBuildCacheDir.mkdir(m_pkgBuildCacheDirPath)) {
        return false;
    }

    //// 2. ????????????????????????
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

    //// 3. ??????DEBIAN???????????????
    QString outputDebianDir = QString("%1/DEBIAN").arg(m_pkgBuildCacheDirPath);
    QFileInfo outputDebianDirInfo(outputDebianDir);
    if (outputDebianDirInfo.dir().exists()) {
        outputDebianDirInfo.dir().mkdir(outputDebianDir);
    }
    // ??????DEBIAN/changelog??????
    QString changelogGZPath = QString("/usr/share/doc/%1/changelog.Debian.gz").arg(info.pkgName);
    QString changelogOutputPath = QString("%1/changelog").arg(outputDebianDir);
    // ????????????????????????gzip
    QMimeDatabase mimeDb;
    QMimeType mimeType;
    // ?????? changelog.Debian.gz ?????????????????????????????????????????????
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

    // ??????DEBIAN/copyright??????
    QString copyrightFilePath = QString("/usr/share/doc/%1/copyright").arg(info.pkgName);
    QString copyrightFileOutputPath = QString("%1/copyright").arg(outputDebianDir);
    if (QFile::exists(copyrightFilePath)) {
        QFile::copy(copyrightFilePath, copyrightFileOutputPath);
    }

    // ??????DEBIAN/postinst??????
    QString postinstFilePath = QString("/var/lib/dpkg/info/%1.postinst").arg(info.pkgName);
    QString postinstFileOutputPath = QString("%1/postinst").arg(outputDebianDir);
    if (QFile::exists(postinstFilePath)) {
        QFile::copy(postinstFilePath, postinstFileOutputPath);
    }
    // ??????DEBIAN/postrm??????
    QString postrmFilePath = QString("/var/lib/dpkg/info/%1.postrm").arg(info.pkgName);
    QString postrmFileOutputPath = QString("%1/postrm").arg(outputDebianDir);
    if (QFile::exists(postrmFilePath)) {
        QFile::copy(postrmFilePath, postrmFileOutputPath);
    }
    // ??????DEBIAN/preinst??????
    QString preinstFilePath = QString("/var/lib/dpkg/info/%1.preinst").arg(info.pkgName);
    QString preinstFileOutputPath = QString("%1/preinst").arg(outputDebianDir);
    if (QFile::exists(preinstFilePath)) {
        QFile::copy(preinstFilePath, preinstFileOutputPath);
    }
    // ??????DEBIAN/prerm??????
    QString prermFilePath = QString("/var/lib/dpkg/info/%1.prerm").arg(info.pkgName);
    QString prermFileOutputPath = QString("%1/prerm").arg(outputDebianDir);
    if (QFile::exists(prermFilePath)) {
        QFile::copy(prermFilePath, prermFileOutputPath);
    }
    // ??????DEBIAN/conffiles??????
    QString conffilesFilePath = QString("/var/lib/dpkg/info/%1.conffiles").arg(info.pkgName);
    QString conffilesFileOutputPath = QString("%1/conffiles").arg(outputDebianDir);
    if (QFile::exists(conffilesFilePath)) {
        QFile::copy(conffilesFilePath, conffilesFileOutputPath);
    }
    // ??????DEBIAN/md5sums??????
    QString md5sumsFilePath = QString("/var/lib/dpkg/info/%1.md5sums").arg(info.pkgName);
    QString md5sumsFileOutputPath = QString("%1/md5sums").arg(outputDebianDir);
    if (QFile::exists(md5sumsFilePath)) {
        QFile::copy(md5sumsFilePath, md5sumsFileOutputPath);
    }
    // ??????DEBIAN/triggers??????
    QString triggersFilePath = QString("/var/lib/dpkg/info/%1.triggers").arg(info.pkgName);
    QString triggersFileOutputPath = QString("%1/triggers").arg(outputDebianDir);
    if (QFile::exists(triggersFilePath)) {
        QFile::copy(triggersFilePath, triggersFileOutputPath);
    }

    //// 4. ??????DEBIAN/control??????
    // ???????????????
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
        // ????????????????????????
        if (lineTxt.startsWith(pkgControlInfosHeader)) {
            isThisPkgInfo = true;
        }
        // ????????????????????????
        if (lineTxt.isEmpty() && isThisPkgInfo) {
            isThisPkgInfo = false;
            break;
        }
        // ??????????????????????????????????????????????????????
        if (isThisPkgInfo) {
            // ?????????Status??????
            if (lineTxt.startsWith("Status: ")) {
                continue;
            }

            pkgControlInfos.append(lineTxt);
            pkgControlInfos.append("\n");
        }
    }
    statusFile.close();
    sync();

    // ???DEBIAN/control????????????
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

    //// 5. ??????
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
    // ????????????????????????
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
