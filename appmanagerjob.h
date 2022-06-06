#pragma once

#include "appmanagercommon.h"
#include "pkgmonitor.h"

#include <QObject>
#include <QMap>
#include <QMutex>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QStandardItemModel;
QT_END_NAMESPACE

#define OH_MY_DDE_PKG_NAME "top.yzzi.youjian"
#define PROC_INFO_PLUGIN_PKG_NAME "com.github.proc-info-plugin"
// 本地安装包路径
#define OH_MY_DDE_LOCAL_PKG_PATH "/opt/apps/com.github.ccc-app-manager/files/pkg/top.yzzi.youjian_1.0.3_amd64.deb"
#define PROC_INFO_PLUGIN_LOCAL_PKG_PATH "/opt/apps/com.github.ccc-app-manager/files/pkg/com.github.proc-info-plugin_0.0.1_amd64.deb"

class AppManagerJob : public QObject
{
    Q_OBJECT
public:
    explicit AppManagerJob(QObject *parent = nullptr);
    virtual ~AppManagerJob() override;

    QMap<QString, AM::AppInfo> getAppInfosMap();

    QList<AM::AppInfo> getSearchedAppInfoList();
    QStandardItemModel *getListViewModel();
    QList<AM::AppInfo> getShowingAppInfoList();
    QString getDownloadDirPath() const;
    QString getPkgBuildDirPath() const;

public Q_SLOTS:
    void init();
    void reloadAppInfos();
    void downloadPkg(const QString &pkgName);
    void downloadFile(const QString &url);
    void onHttpReadyRead();
    void onDownloadProgressChanged(qint64, qint64);
    void onFileDownloadFinished();
    // 开始构建安装包任务
    void startBuildPkgTask(const AM::AppInfo &info);

    // 开始搜索任务
    void startSearchTask(const QString &text);
    // 创建列表数据模型
    void createListViewMode(const QList<AM::AppInfo> &list);

    void uninstallPkg(const QString &pkgName);
    void installOhMyDDE();
    void installProcInfoPlugin();

private Q_SLOTS:
    // 包安装变动
    void onPkgInstalled(const QString &pkgName);
    void onPkgUpdated(const QString &pkgName);
    void onPkgUninstalled(const QString &pkgName);

Q_SIGNALS:
    void loadAppInfosFinished();
    void downloadPkgFinished(const QString &pkgName);
    void fileDownloadProgressChanged(const QString &url, qint64 bytesRead, qint64 totalBytes);
    void fileDownloadFinished(const QString &url);

    // 搜索任务完成
    void searchTaskFinished();
    // 创建列表数据模型完成
    void createListViewModeFinished();

    void uninstallPkgFinished(const QString &pkgName);
    // 构建安装包任务完成
    void buildPkgTaskFinished(bool successed, const AM::AppInfo &info);
    void installOhMyDDEFinished(bool successed);
    void installProcInfoPluginFinished(bool successed);
    // 软件安装变动
    void appInstalled(const AM::AppInfo &appInfo);
    void appUpdated(const AM::AppInfo &appInfo);
    void appUninstalled(const AM::AppInfo &appInfo);

private:
    void initConnection();
    QList<QString> readSourceUrlList(const QString &filePath);
    void reloadSourceUrlList();
    // 从包信息列表文件中获取包信息列表
    bool getPkgInfoListFromFile(QList<AM::PkgInfo> &pkgInfoList, const QString &pkgInfosFilePath, bool isCompact = false);
    // 从本地包信息列表文件中获取某个包信息
    bool getInstalledPkgInfo(AM::PkgInfo &pkgInfo, const QString &pkgName);

    // 从包信息列表中加载仓库应用信息列表
    void loadSrvAppInfosFromFile(QMap<QString, AM::AppInfo> &appInfosMap, const QString &pkgInfosFilePath);
    // 加载包的已安装软件信息
    void loadPkgInstalledAppInfo(const AM::PkgInfo &pkgInfo);
    // 从包信息列表中加载已安装应用信息列表
    void loadAllPkgInstalledAppInfos();

    QStringList getAppInstalledFileList(const QString &pkgName);
    QString getAppDesktopPath(const QStringList &list, const QString &pkgName);
    AM::DesktopInfo getDesktopInfo(const QString &desktop);

    qint64 getUrlFileSize(QString &url, int tryTimes = 3);

    // 由应用信息列表获得标准单元数据类
    QStandardItemModel *getItemModelFromAppInfoList(const QList<AM::AppInfo> &appInfoList);
    // 构建安装包任务
    bool buildPkg(const AM::AppInfo &info);
    // 安全本地软件包
    bool installLocalPkg(const QString &path);

private:
    QMutex m_mutex;
    QList<QString> m_sourceUrlList;
    QMap<QString, AM::AppInfo> m_appInfosMap;

    bool m_isInitiallized;
    QString m_downloadDirPath;
    QFile *m_downloadingFile;
    QNetworkAccessManager *m_netManager;
    QNetworkReply *m_netReply;
    QString m_downloadingFileOriginUrl;

    QList<AM::AppInfo> m_searchedAppInfoList;
    // 当前正在使用的列表数据模型
    QStandardItemModel *m_listViewModel;
    QList<AM::AppInfo> m_showingAppInfoList;
    // deb构建缓存目录
    QString m_pkgBuildCacheDirPath;
    // deb构建目录
    QString m_pkgBuildDirPath;
    // 包监视器
    PkgMonitor *m_pkgMonitor;
};
