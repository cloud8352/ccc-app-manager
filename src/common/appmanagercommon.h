#pragma once

#include <DStyleOption>

#include <QGSettings/QGSettings>

#define ONLY_SHOW_IN_VALUE_DEEPIN "Deepin"
#define X_DEEPIN_VENDOR_STR "deepin"
// 应用默认图标
#define APP_THEME_ICON_DEFAULT "application-x-executable"
#define MY_PKG_NAME "com.github.ccc-app-manager"

// 列表数据角色定义
#define AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME Dtk::ItemDataRole::UserRole + 1

// 文件大小单位，以b为基本单位
#define KB_COUNT (1 << 10)
#define MB_COUNT (1 << 20)
#define GB_COUNT (1 << 30)
#define TB_COUNT (long(1) << 40)

// 时间格式化字符串
#define DATE_TIME_FORMAT_STR "yyyy-MM-dd HH:mm:ss.zzz"

namespace AM {
// 运行状态
enum RunningStatus {
    Normal = 0,
    Busy
};

struct PkgInfo {
    QString infosFilePath; // 包信息文件路径
    qint64 contentOffset; // 内容在包信息文件中的地址偏移
    qint64 contentSize; // 内容大小
    QString depositoryUrl; // 仓库地址
    QString pkgName;
    bool isInstalled;
    bool isHoldVersion; // 是否保持版本
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
        isHoldVersion = false;
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

void popupNormalSysNotify(const QString &summary, const QString &body);
// 格式化容量
QString formatBytes(qint64 input, int prec);
// 从状态字符串中判断包是否已安装
bool judgePkgIsInstalledFromStr(const QString &str);
// 判断GSettings中是否包含指定键
bool isQGSettingsContainsKey(const QGSettings &settings, const QString &key);
} // namespace AM
