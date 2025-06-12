#pragma once

#include "common/appmanagercommon.h"
#include "job/appmanagerjob.h"

#include <DSysInfo>

#include <QObject>
#include <QMap>
#include <QDBusInterface>
#include <QSettings>
#include <QTextCodec>

DCORE_USE_NAMESPACE

class QStandardItemModel;

#define APP_TYPE_INSTALLED 0 // 应用类型-已安装
#define APP_TYPE_ONLINE 1 // 应用类型-在线

class AppManagerModel : public QObject
{
    Q_OBJECT
public:
    explicit AppManagerModel(QObject *parent = nullptr);
    virtual ~AppManagerModel() override;

    bool IsInGxdeOs();

    RunningStatus getRunningStatus();

    QList<AM::AppInfo> getAppInfosList();
    QString formatePkgInfo(const AM::PkgInfo &info);

    QList<AM::AppInfo> getSearchedAppInfoList() const;

    void openStoreAppDetailPage(const QString &pkgName);
    void openSpkStoreAppDetailPage(const QString &pkgName);
    QString getDownloadDirPath() const;
    QString getPkgBuildDirPath() const;
    // 拓展包信息
    bool extendPkgInfo(AM::PkgInfo &pkgInfo);
    // 软件包是否已安装
    bool isPkgInstalled(const QString &pkgName);
    // 获取应用信息
    AM::AppInfo getAppInfo(const QString &pkgName);
    // 运行desktop执行命令
    void startDetachedDesktopExec(const QString &exec);

    void showFileItemInFileManager(const QString &urlPath);

Q_SIGNALS:
    void runningStatusChanged(RunningStatus status);

    void notifyThreadreloadAppInfos();
    void loadAppInfosFinished();
    void notifyThreadDownloadPkgFile(const PkgInfo &info);
    void pkgFileDownloadProgressChanged(const PkgInfo &info, qint64 bytesRead, qint64 totalBytes);
    void pkgFileDownloadFinished(const PkgInfo &info);
    void pkgFileDownloadFailed(const PkgInfo &info);
    void notifyThreadStartSearchTask(const QString &text);
    // 搜索任务完成
    void searchTaskFinished();
    // 通知卸载包
    void notifyThreadUninstallPkg(const QString &pkgName);
    // 通知构建安装包
    void notifyThreadBuildPkg(const AM::AppInfo &info, bool withDepends);
    // 构建安装包任务完成
    void buildPkgTaskFinished(bool successed, const AM::AppInfo &info);
    // 通知安装oh-my-dde
    void notifyThreadInstallOhMyDDE();
    // 安装oh-my-dde完成
    void installOhMyDDEFinished(bool successed, const QString &err);
    // 通知打开星火应用商店需要安装对话框
    void notifyOpenSparkStoreNeedBeInstallDlg();
    // 软件安装变动
    void appInstalled(const AM::AppInfo &appInfo);
    void appUpdated(const AM::AppInfo &appInfo);
    void appUninstalled(const AM::AppInfo &appInfo);
    // 通知线程保持软件包版本
    void notigyThreadHoldPkgVersion(const QString &pkgName, bool hold);

private Q_SLOTS:
    // 软件安装变动
    void onAppInstalled(const AM::AppInfo &appInfo);
    void onAppUpdated(const AM::AppInfo &appInfo);
    void onAppUninstalled(const AM::AppInfo &appInfo);

private:
    void initData();
    void initConnection();
    void postInit();
    void readOsInfo();

private:
    AppManagerJob *m_appManagerJob;
    QThread *m_appManagerJobThread;
    QString m_osId;
};
