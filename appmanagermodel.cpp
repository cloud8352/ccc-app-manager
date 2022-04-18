#include "appmanagermodel.h"
#include "appmanagerjob.h"
#include <qurl.h>
#include <QThread>
#include <QDebug>
#include <QProcess>
#include <QStringList>

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

void AppManagerModel::setShowingAppInfo(const AM::AppInfo &info)
{
    m_showingAppInfo = info;
}

AppInfo AppManagerModel::getShowingAppInfo()
{
    return m_showingAppInfo;
}

QList<AppInfo> AppManagerModel::getSearchedAppInfoList() const
{
    return m_appManagerJob->getSearchedAppInfoList();
}

QStandardItemModel *AppManagerModel::getListViewModel()
{
    return m_appManagerJob->getListViewModel();
}

QList<AppInfo> AppManagerModel::getShowingAppInfoList() const
{
    return m_appManagerJob->getShowingAppInfoList();
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
    QProcess spkopen;
    QString cmd = QString("notify-send shenmo \"\因为无法获取分类信息，暂时没有实现这个功能\"\ ");
    spkopen.start(cmd);
    spkopen.waitForStarted();
    spkopen.waitForFinished();

}

QString AppManagerModel::getDownloadDirPath() const
{
    return m_appManagerJob->getDownloadDirPath();
}

QString AppManagerModel::getPkgBuildDirPath() const
{
    return m_appManagerJob->getPkgBuildDirPath();
}

void AppManagerModel::initData()
{
    // 注册结构体
    qRegisterMetaType<AM::AppInfo>("AM::AppInfo");
    qRegisterMetaType<QList<AM::AppInfo>>("QList<AM::AppInfo>");

    // 线程
    m_appManagerJobThread = new QThread;
    m_appManagerJob = new AppManagerJob;
    m_appManagerJob->moveToThread(m_appManagerJobThread);
}

void AppManagerModel::initConnection()
{
    // 线程信号连接
    connect(m_appManagerJobThread, &QThread::started, m_appManagerJob, &AppManagerJob::init);
    connect(this, &AppManagerModel::notifyThreadreloadAppInfos, m_appManagerJob, &AppManagerJob::reloadAppInfos);
    connect(this, &AppManagerModel::notifyThreadDownloadFile, m_appManagerJob, &AppManagerJob::downloadFile);
    connect(m_appManagerJob, &AppManagerJob::fileDownloadProgressChanged, this, [this](const QString &url, qint64 bytesRead, qint64 totalBytes) {
        Q_EMIT this->fileDownloadProgressChanged(url, bytesRead, totalBytes);
    });
    connect(m_appManagerJob, &AppManagerJob::fileDownloadFinished, this, [this](const QString &url) {
        Q_EMIT this->fileDownloadFinished(url);
    });
    connect(m_appManagerJob, &AppManagerJob::loadAppInfosFinished, this, [this] {
        Q_EMIT this->loadAppInfosFinished();
    });

    connect(this, &AppManagerModel::notifyThreadStartSearchTask, m_appManagerJob, &AppManagerJob::startSearchTask);
    connect(m_appManagerJob, &AppManagerJob::searchTaskFinished, this, [this]() {
        Q_EMIT this->searchTaskFinished();
    });

    connect(this, &AppManagerModel::notifyThreadCreateListViewMode, m_appManagerJob, &AppManagerJob::createListViewMode);
    connect(m_appManagerJob, &AppManagerJob::createListViewModeFinished, this, [this]() {
        Q_EMIT this->createListViewModeFinished();
    });

    connect(this, &AppManagerModel::notifyThreadUninstallPkg, m_appManagerJob, &AppManagerJob::uninstallPkg);
    connect(m_appManagerJob, &AppManagerJob::uninstallPkgFinished, this, [this](const QString &pkgName) {
        QProcess uninstnotify;
            QString cmd = QString("notify-send ccc-app-manager \"\软件包 ")+pkgName+QString(" 已卸载\"\ ");
            uninstnotify.start(cmd);
            uninstnotify.waitForStarted();
            uninstnotify.waitForFinished();
        qInfo() << pkgName << "uninstalled";
    });

    // 通知构建安装包
    connect(this, &AppManagerModel::notifyThreadBuildPkg, m_appManagerJob, &AppManagerJob::startBuildPkgTask);
    // 构建安装包任务完成
    connect(m_appManagerJob, &AppManagerJob::buildPkgTaskFinished, this, &AppManagerModel::buildPkgTaskFinished);
}

void AppManagerModel::postInit()
{
    // 启动线程
    m_appManagerJobThread->start();
}
