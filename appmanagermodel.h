#pragma once

#include "appmanagercommon.h"
#include "appmanagerjob.h"

#include <QObject>
#include <QMap>

class QStandardItemModel;

#define APP_TYPE_INSTALLED 0 // 应用类型-已安装
#define APP_TYPE_ONLINE 1 // 应用类型-在线

class AppManagerModel : public QObject
{
    Q_OBJECT
public:
    explicit AppManagerModel(QObject *parent = nullptr);
    virtual ~AppManagerModel() override;

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
    void notifyThreadBuildPkg(const AM::AppInfo &info);
    // 构建安装包任务完成
    void buildPkgTaskFinished(bool successed, const AM::AppInfo &info);
    // 通知安装oh-my-dde
    void notifyThreadInstallOhMyDDE();
    // 安装oh-my-dde完成
    void installOhMyDDEFinished(bool successed);
    // 通知安装proc-info-plugin
    void notifyThreadInstallProcInfoPlugin();
    // 安装proc-info-plugin完成
    void installProcInfoPluginFinished(bool successed);
    // 软件安装变动
    void appInstalled(const AM::AppInfo &appInfo);
    void appUpdated(const AM::AppInfo &appInfo);
    void appUninstalled(const AM::AppInfo &appInfo);

private Q_SLOTS:
    // 软件安装变动
    void onAppInstalled(const AM::AppInfo &appInfo);
    void onAppUpdated(const AM::AppInfo &appInfo);
    void onAppUninstalled(const AM::AppInfo &appInfo);

private:
    void initData();
    void initConnection();
    void postInit();

private:
    AppManagerJob *m_appManagerJob;
    QThread *m_appManagerJobThread;
};
