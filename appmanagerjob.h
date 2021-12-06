#pragma once

#include "appmanagercommon.h"

#include <QObject>
#include <QMap>
#include <QMutex>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QStandardItemModel;
QT_END_NAMESPACE

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

private:
    QList<QString> readSourceUrlList(const QString &filePath);
    void reloadSourceUrlList();
    // 从包信息列表文件中获取包信息列表
    bool getPkgInfoListFromFile(QList<AM::PkgInfo> &pkgInfoList, const QString &pkgInfosFilePath);

    // 从包信息列表中加载仓库应用信息列表
    void loadSrvAppInfosFromFile(QMap<QString, AM::AppInfo> &appInfosMap, const QString &pkgInfosFilePath);
    // 从包信息列表中加载已安装应用信息列表
    void loadInstalledAppInfosFromFile(QMap<QString, AM::AppInfo> &appInfosMap, const QString &pkgInfosFilePath);

    QStringList getAppInstalledFileList(const QString &pkgName);
    QString getAppDesktopPath(const QStringList &list, const QString &pkgName);
    AM::DesktopInfo getDesktopInfo(const QString &desktop);

    qint64 getUrlFileSize(QString &url, int tryTimes = 3);

    // 由应用信息列表获得标准单元数据类
    QStandardItemModel *getItemModelFromAppInfoList(const QList<AM::AppInfo> &appInfoList);
    // 构建安装包任务
    bool buildPkg(const AM::AppInfo &info);

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
};
