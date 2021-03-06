#pragma once

#include <QObject>

#define ONLY_SHOW_IN_VALUE_DEEPIN "Deepin"
#define X_DEEPIN_VENDOR_STR "deepin"
#define APP_THEME_ICON_DEFAULT "application-x-executable"

namespace AM {
struct PkgInfo {
    QString pkgName;
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
        installedSize = 0;
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
