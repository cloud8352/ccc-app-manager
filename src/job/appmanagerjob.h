#pragma once

#include "../common/appmanagercommon.h"
#include "../pkgmonitor/pkgmonitor.h"

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

using namespace AM;

class AppManagerJob : public QObject
{
    Q_OBJECT
public:
    explicit AppManagerJob(QObject *parent = nullptr);
    virtual ~AppManagerJob() override;

    RunningStatus getRunningStatus();

    QMap<QString, AM::AppInfo> getAppInfosMap();

    QList<AM::AppInfo> getSearchedAppInfoList();
    QString getDownloadDirPath() const;
    QString getPkgBuildDirPath() const;

public Q_SLOTS:
    void init();
    void reloadAppInfos();
    void downloadPkg(const QString &pkgName);
    void downloadPkgFile(const PkgInfo &info);
    void downloadFile(const QString &url);
    void onHttpReadyRead();
    void onDownloadProgressChanged(qint64, qint64);
    void onFileDownloadFinished();
    // 开始构建安装包任务
    void startBuildPkgTask(const AM::AppInfo &info);

    // 开始搜索任务
    void startSearchTask(const QString &text);

    void uninstallPkg(const QString &pkgName);
    void installOhMyDDE();
    void installProcInfoPlugin();

    // 保持软件包版本
    void holdPkgVersion(const QString &pkgName, bool hold);

private Q_SLOTS:
    // 包安装变动
    void onPkgInstalled(const QString &pkgName);
    void onPkgUpdated(const QString &pkgName);
    void onPkgUninstalled(const QString &pkgName);

Q_SIGNALS:
    void runningStatusChanged(RunningStatus status);
    void loadAppInfosFinished();
    void downloadPkgFinished(const QString &pkgName);
    void pkgFileDownloadProgressChanged(const PkgInfo &info, qint64 bytesRead, qint64 totalBytes);
    void pkgFileDownloadFinished(const PkgInfo &info);
    void pkgFileDownloadFailed(const PkgInfo &info);

    // 搜索任务完成
    void searchTaskFinished();

    void uninstallPkgFinished(const QString &pkgName);
    // 构建安装包任务完成
    void buildPkgTaskFinished(bool successed, const AM::AppInfo &info);
    void installOhMyDDEFinished(bool successed, const QString &err);
    void installProcInfoPluginFinished(bool successed, const QString &err);
    // 软件安装变动
    void appInstalled(const AM::AppInfo &appInfo);
    void appUpdated(const AM::AppInfo &appInfo);
    void appUninstalled(const AM::AppInfo &appInfo);

private:
    void initConnection();
    void setRunningStatus(RunningStatus status);

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
    QStringList getAppDesktopPathList(const QStringList &list, const QString &pkgName);
    AM::DesktopInfo getDesktopInfo(const QString &desktop);

    qint64 getUrlFileSize(QString &url, int tryTimes = 3);

    // 构建安装包任务
    bool buildPkg(const AM::AppInfo &info);
    // 安全本地软件包
    bool installLocalPkg(const QString &path, QString &err);

private:
    QMutex m_mutex;
    RunningStatus m_runningStatus;
    QList<QString> m_sourceUrlList;
    bool m_isOnlyLoadCurrentArchAppInfos;
    QMap<QString, AM::AppInfo> m_appInfosMap;

    bool m_isInitiallized;
    QString m_downloadDirPath;
    QFile *m_downloadingFile;
    QNetworkAccessManager *m_netManager;
    QNetworkReply *m_netReply;
    PkgInfo m_downloadingPkgInfo;

    QList<AM::AppInfo> m_searchedAppInfoList;
    // deb构建缓存目录
    QString m_pkgBuildCacheDirPath;
    // deb构建目录
    QString m_pkgBuildDirPath;
    // 包监视器
    PkgMonitor *m_pkgMonitor;
};
