#pragma once

#include <QObject>

#define ONLY_SHOW_IN_VALUE_DEEPIN "Deepin"
#define X_DEEPIN_VENDOR_STR "deepin"
#define APP_THEME_ICON_DEFAULT "application-x-executable"

// 列表数据角色定义
#define AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME Qt::ItemDataRole::UserRole + 1

namespace AM {
struct PkgInfo {
    QString infosFilePath; // 包信息文件路径
    qint64 contentOffset; // 内容在包信息文件中的地址偏移
    qint64 contentSize; // 内容大小
    QString depositoryUrl; // 仓库地址
    QString pkgName;
    bool isInstalled;
    int installedSize;
    QString updatedTime;
    QString maintainer;
    QString arch;
    QString version;
    QString downloadUrl;
    int pkgSize;
    QString homepage;
    QString depends;
    QString description;
    QStringList installedFileList;
    PkgInfo()
    {
        contentOffset = 0;
        contentSize = 0;
        installedSize = 0;
        isInstalled = false;
        pkgSize = 0;
    }
};
struct DesktopInfo {
    QString appName;
    QString desktopPath;
    QString execPath;
    QString exec;
    bool isSysApp;
    QString themeIconName;
    DesktopInfo()
    {
        isSysApp = true;
    }
};

struct AppInfo {
    QString pkgName; // 包名作为唯一识别信息
    QList<PkgInfo> pkgInfoList;
    bool isInstalled;
    PkgInfo installedPkgInfo;
    DesktopInfo desktopInfo;
    AppInfo()
    {
        isInstalled = false;
    }

    bool operator== (const AppInfo &info) {
        return this->pkgName == info.pkgName;
    }
};

// 拼音信息
struct PinyinInfo {
    QString normalPinYin;
    QString noTonePinYin;
    QString simpliyiedPinYin;
};

// 判断是否为汉字
bool isChineseChar(const QChar &character);
// 字符串转拼音
PinyinInfo getPinYinInfoFromStr(const QString &words);
} // namespace AM
