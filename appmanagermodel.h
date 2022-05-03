#pragma once

#include "appmanagercommon.h"

#include <QObject>
#include <QMap>

class AppManagerJob;

class QStandardItemModel;

#define APP_TYPE_INSTALLED 0 // 应用类型-已安装
#define APP_TYPE_ONLINE 1 // 应用类型-在线

class AppManagerModel : public QObject
{
    Q_OBJECT
public:
    explicit AppManagerModel(QObject *parent = nullptr);
    virtual ~AppManagerModel() override;

    QList<AM::AppInfo> getAppInfosList();
    QString formatePkgInfo(const AM::PkgInfo &info);

    void setShowingAppInfo(const AM::AppInfo &info);
    AM::AppInfo getShowingAppInfo();

    QList<AM::AppInfo> getSearchedAppInfoList() const;
    QStandardItemModel *getListViewModel();
    QList<AM::AppInfo> getShowingAppInfoList() const;

    void openStoreAppDetailPage(const QString &pkgName);
    void openSpkStoreAppDetailPage(const QString &pkgName);
    QString getDownloadDirPath() const;
    QString getPkgBuildDirPath() const;
    // 拓展包信息
    bool extendPkgInfo(AM::PkgInfo &pkgInfo);

Q_SIGNALS:
    void notifyThreadreloadAppInfos();
    void loadAppInfosFinished();
    void notifyThreadDownloadFile(const QString &url);
    void fileDownloadProgressChanged(const QString &url, qint64 bytesRead, qint64 totalBytes);
    void fileDownloadFinished(const QString &url);
    void notifyThreadStartSearchTask(const QString &text);
    // 搜索任务完成
    void searchTaskFinished();
    // 通知创建列表数据模型
    void notifyThreadCreateListViewMode(const QList<AM::AppInfo> &list);
    // 创建列表数据模型完成
    void createListViewModeFinished();
    // 通知卸载包
    void notifyThreadUninstallPkg(const QString &pkgName);
    // 通知构建安装包
    void notifyThreadBuildPkg(const AM::AppInfo &info);
    // 构建安装包任务完成
    void buildPkgTaskFinished(bool successed, const AM::AppInfo &info);

private:
    void initData();
    void initConnection();
    void postInit();

private:
    AM::AppInfo m_showingAppInfo;

    AppManagerJob *m_appManagerJob;
    QThread *m_appManagerJobThread;
};
