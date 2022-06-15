#include "appmanagermodel.h"
#include <qurl.h>
#include <QThread>
#include <QDebug>
#include <QProcess>
#include <QStringList>
#include <QFile>

using namespace AM;

AppManagerModel::AppManagerModel(QObject *parent)
    : QObject(parent)
    , m_appManagerJob(nullptr)
    , m_appManagerJobThread(nullptr)
{
    initData();
    initConnection();
    postInit();
}

AppManagerModel::~AppManagerModel()
{
    m_appManagerJobThread->quit();
    m_appManagerJobThread->wait();
    m_appManagerJobThread->deleteLater();
    m_appManagerJobThread = nullptr;

    m_appManagerJob->deleteLater();
    m_appManagerJob = nullptr;
}

RunningStatus AppManagerModel::getRunningStatus()
{
    return m_appManagerJob->getRunningStatus();
}

QList<AM::AppInfo> AppManagerModel::getAppInfosList()
{
    return m_appManagerJob->getAppInfosMap().values();
}

QString AppManagerModel::formatePkgInfo(const PkgInfo &info)
{
    QString text = QString("应用名：%1\n包名：%2\n安装后占用：%3KB\n更新时间：%4\n维护者：%5\n"
                           "架构：%6\n版本：%7\n下载地址：%8\n安装包大小：%9B\n主页：%10\n"
                           "桌面文件：%11\n执行命令：%12\n执行路径：%13\n依赖：%14\n描述：%15\n\n")
                       .arg("")
                       .arg(info.pkgName)
                       .arg(info.installedSize)
                       .arg(info.updatedTime)
                       .arg(info.maintainer)
                       .arg(info.arch)
                       .arg(info.version)
                       .arg(info.downloadUrl)
                       .arg(info.pkgSize)
                       .arg(info.homepage)
                       .arg("")
                       .arg("")
                       .arg("")
                       .arg(info.depends)
                       .arg(info.description);

    return text;
}

QList<AppInfo> AppManagerModel::getSearchedAppInfoList() const
{
    return m_appManagerJob->getSearchedAppInfoList();
}

void AppManagerModel::openStoreAppDetailPage(const QString &pkgName)
{
    QProcess proc;
    QString cmd = QString("qdbus com.home.appstore.client "
                          "/com/home/appstore/client "
                          "com.home.appstore.client.openBusinessUri "
                          "\"app_detail_info/%1\"")
                      .arg(pkgName);
    proc.start(cmd);
    proc.waitForStarted();
    proc.waitForFinished();

    cmd = QString("qdbus com.home.appstore.client "
                  "/com/home/appstore/client "
                  "com.home.appstore.client.newInstence");
    proc.start(cmd);
    proc.waitForStarted();
    proc.waitForFinished();
}

void AppManagerModel::openSpkStoreAppDetailPage(const QString &pkgName)
{
    Q_UNUSED(pkgName);
    popupNormalSysNotify("shenmo", "因为无法获取分类信息，暂时没有实现这个功能");
}

QString AppManagerModel::getDownloadDirPath() const
{
    return m_appManagerJob->getDownloadDirPath();
}

QString AppManagerModel::getPkgBuildDirPath() const
{
    return m_appManagerJob->getPkgBuildDirPath();
}

bool AppManagerModel::extendPkgInfo(PkgInfo &pkgInfo)
{
    QFile pkgInfosFile(pkgInfo.infosFilePath);

    if (!pkgInfosFile.open(QIODevice::OpenModeFlag::ReadOnly)) {
        qInfo() << Q_FUNC_INFO << "open" << pkgInfosFile.fileName() << "failed!";
        return false;
    }

    pkgInfosFile.seek(pkgInfo.contentOffset);
    QString content = pkgInfosFile.read(pkgInfo.contentSize);
    pkgInfosFile.close();

    QStringList infoLineList = content.split("\n");
    bool isReadingDescription = false;
    for (const QString &infoLine : infoLineList) {
        if (infoLine.startsWith("Package: ")) {
            pkgInfo.pkgName = infoLine.split(": ").last();
            continue;
        }

        if (infoLine.startsWith("Installed-Size: ")) {
            pkgInfo.installedSize = infoLine.split(": ").last().toInt();
            continue;
        }
        if (infoLine.startsWith("Maintainer: ")) {
            pkgInfo.maintainer = infoLine.split(": ").last();
            continue;
        }
        if (infoLine.startsWith("Architecture: ")) {
            pkgInfo.arch = infoLine.split(": ").last();
            continue;
        }
        if (infoLine.startsWith("Version: ")) {
            pkgInfo.version = infoLine.split(": ").last();
            continue;
        }
        if (infoLine.startsWith("Depends: ")) {
            pkgInfo.depends = infoLine.split(": ").last();
            continue;
        }
        if (infoLine.startsWith("Filename: ")) {
            const QString downloadFileName = infoLine.split(": ").last();
            pkgInfo.downloadUrl = QString("%1/%2").arg(pkgInfo.depositoryUrl).arg(downloadFileName);
            continue;
        }
        if (infoLine.startsWith("Size: ")) {
            pkgInfo.pkgSize = infoLine.split(": ").last().toInt();
            continue;
        }

        if (infoLine.startsWith("Homepage: ")) {
            pkgInfo.homepage = infoLine.split(": ").last();
            continue;
        }


        if (infoLine.startsWith("Description: ")) {
            pkgInfo.description = infoLine.split(": ").last();
            pkgInfo.description.append("\n");
            isReadingDescription = true;
            continue;
        }
        if (infoLine.startsWith(" ") && isReadingDescription) {
            pkgInfo.description += infoLine;
            continue;
        }
        if (infoLine.startsWith("Build-Depends: ")) {
            isReadingDescription = false;
            continue;
        }
    }

    return true;
}

bool AppManagerModel::isPkgInstalled(const QString &pkgName)
{
    const QList<AppInfo> &appList = m_appManagerJob->getAppInfosMap().values();
    for (QList<AppInfo>::const_iterator iter = appList.cbegin();
         iter != appList.cend(); ++iter) {
        if (!iter->isInstalled) {
            continue;
        }
        if (pkgName == iter->pkgName) {
            return true;
        }
    }

    return false;
}

AppInfo AppManagerModel::getAppInfo(const QString &pkgName)
{
    return m_appManagerJob->getAppInfosMap().value(pkgName);
}

void AppManagerModel::startDetachedDesktopExec(const QString &exec)
{
    QString execAdjusted = exec;
    QStringList execList = exec.split(" ");
    if (execList.last().startsWith("%")) {
        execAdjusted = execAdjusted.remove(execList.last());
    }

    QProcess proc;
    proc.startDetached("/usr/bin/bash", {"-c", execAdjusted});
    proc.close();
}

void AppManagerModel::onAppInstalled(const AppInfo &appInfo)
{
    popupNormalSysNotify("ccc-app-manager", QString("软件包 %1 已安装").arg(appInfo.pkgName));

    Q_EMIT appInstalled(appInfo);
}

void AppManagerModel::onAppUpdated(const AppInfo &appInfo)
{
    popupNormalSysNotify("ccc-app-manager", QString("软件包 %1 已更新").arg(appInfo.pkgName));

    Q_EMIT appUpdated(appInfo);
}

void AppManagerModel::onAppUninstalled(const AppInfo &appInfo)
{
    popupNormalSysNotify("ccc-app-manager", QString("软件包 %1 已卸载").arg(appInfo.pkgName));

    Q_EMIT appUninstalled(appInfo);
}

void AppManagerModel::initData()
{
    // 注册结构体
    qRegisterMetaType<AM::AppInfo>("AM::AppInfo");
    qRegisterMetaType<QList<AM::AppInfo>>("QList<AM::AppInfo>");
    qRegisterMetaType<AM::RunningStatus>("AM::RunningStatus");
    qRegisterMetaType<RunningStatus>("RunningStatus");
    qRegisterMetaType<PkgInfo>("PkgInfo");
    qRegisterMetaType<AM::PkgInfo>("AM::PkgInfo");
    qRegisterMetaType<QList<PkgInfo>>("QList<PkgInfo>");
    qRegisterMetaType<QList<AM::PkgInfo>>("QList<AM::PkgInfo>");

    // 线程
    m_appManagerJobThread = new QThread;
    m_appManagerJob = new AppManagerJob;
    m_appManagerJob->moveToThread(m_appManagerJobThread);
}

void AppManagerModel::initConnection()
{
    // 线程信号连接
    connect(m_appManagerJob, &AppManagerJob::runningStatusChanged, this, &AppManagerModel::runningStatusChanged);

    connect(m_appManagerJobThread, &QThread::started, m_appManagerJob, &AppManagerJob::init);
    connect(this, &AppManagerModel::notifyThreadreloadAppInfos, m_appManagerJob, &AppManagerJob::reloadAppInfos);
    connect(this, &AppManagerModel::notifyThreadDownloadPkgFile, m_appManagerJob, &AppManagerJob::downloadPkgFile);
    connect(m_appManagerJob, &AppManagerJob::pkgFileDownloadProgressChanged, this, [this](const PkgInfo &info, qint64 bytesRead, qint64 totalBytes) {
        Q_EMIT this->pkgFileDownloadProgressChanged(info, bytesRead, totalBytes);
    });
    connect(m_appManagerJob, &AppManagerJob::pkgFileDownloadFinished, this, [this](const PkgInfo &info) {
        Q_EMIT this->pkgFileDownloadFinished(info);
    });
    connect(m_appManagerJob, &AppManagerJob::pkgFileDownloadFailed, this, &AppManagerModel::pkgFileDownloadFailed);
    connect(m_appManagerJob, &AppManagerJob::loadAppInfosFinished, this, [this] {
        Q_EMIT this->loadAppInfosFinished();
    });

    connect(this, &AppManagerModel::notifyThreadStartSearchTask, m_appManagerJob, &AppManagerJob::startSearchTask);
    connect(m_appManagerJob, &AppManagerJob::searchTaskFinished, this, [this]() {
        Q_EMIT this->searchTaskFinished();
    });

    connect(this, &AppManagerModel::notifyThreadUninstallPkg, m_appManagerJob, &AppManagerJob::uninstallPkg);
    connect(m_appManagerJob, &AppManagerJob::uninstallPkgFinished, this, [](const QString &pkgName) {
        qInfo() << pkgName << "uninstalled";
    });

    // 通知构建安装包
    connect(this, &AppManagerModel::notifyThreadBuildPkg, m_appManagerJob, &AppManagerJob::startBuildPkgTask);
    // 构建安装包任务完成
    connect(m_appManagerJob, &AppManagerJob::buildPkgTaskFinished, this, &AppManagerModel::buildPkgTaskFinished);
    // 通知安装oh-my-dde
    connect(this, &AppManagerModel::notifyThreadInstallOhMyDDE, m_appManagerJob, &AppManagerJob::installOhMyDDE);
    // 安装oh-my-dde完成
    connect(m_appManagerJob, &AppManagerJob::installOhMyDDEFinished, this, &AppManagerModel::installOhMyDDEFinished);
    // 通知安装proc-info-plugin
    connect(this, &AppManagerModel::notifyThreadInstallProcInfoPlugin, m_appManagerJob, &AppManagerJob::installProcInfoPlugin);
    // 安装proc-info-plugin完成
    connect(m_appManagerJob, &AppManagerJob::installProcInfoPluginFinished, this, &AppManagerModel::installProcInfoPluginFinished);

    // 包安装变动
    connect(m_appManagerJob, &AppManagerJob::appInstalled, this, &AppManagerModel::onAppInstalled);
    connect(m_appManagerJob, &AppManagerJob::appUpdated, this, &AppManagerModel::onAppUpdated);
    connect(m_appManagerJob, &AppManagerJob::appUninstalled, this, &AppManagerModel::onAppUninstalled);
}

void AppManagerModel::postInit()
{
    // 启动线程
    m_appManagerJobThread->start();
}
