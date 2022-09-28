#include "appmanagercommon.h"

#include <DtkCores>

// 汉字方法相关
const QRegularExpression OnlyNumRegular = QRegularExpression("^[0-9]+$");

bool AM::isChineseChar(const QChar &character)
{
    ushort unicode = character.unicode();
    if (unicode >= 0x4E00 && unicode <= 0x9FFF) {
        return true;
    }
    return false;
}

AM::PinyinInfo AM::getPinYinInfoFromStr(const QString &words)
{
    PinyinInfo result;
    for (const QChar &singleChar : words) {
        QString singlePinyin = Dtk::Core::Chinese2Pinyin(singleChar);
        result.normalPinYin.append(singlePinyin);

        // 如果非汉字，则不转换
        if (!isChineseChar(singleChar)) {
            result.noTonePinYin.append(singleChar);
            result.simpliyiedPinYin.append(singleChar);
            continue;
        }
        // 无声调拼音
        QString singleNoTonePinyin;
        // 去除声调
        for (const QChar &singlePinyinChar : singlePinyin) {
            QRegularExpressionMatch match = OnlyNumRegular.match(singlePinyinChar, 0);
            if (match.hasMatch()) {
                continue;
            }
            singleNoTonePinyin.append(singlePinyinChar);
        }
        result.noTonePinYin.append(singleNoTonePinyin);

        // 首字母
        result.simpliyiedPinYin.append(singlePinyin.front());
    }

    return result;
}

void AM::popupNormalSysNotify(const QString &summary, const QString &body)
{
    QProcess proc;
    proc.startDetached("notify-send", {"-a", "com.github.ccc-app-manager", summary, body});
}

// 格式化容量
QString AM::formatBytes(qint64 input, int prec)
{
    QString flowValueStr;
    if (KB_COUNT > input) {
        flowValueStr = QString::number(input / 1, 'd', prec) + " B";
    } else if (MB_COUNT > input) {
        flowValueStr = QString::number(input / KB_COUNT + double(input % KB_COUNT) / KB_COUNT, 'd', prec) + " KB";
    } else if (GB_COUNT > input) {
        flowValueStr = QString::number(input / MB_COUNT + double(input % MB_COUNT) / MB_COUNT, 'd', prec) + " MB";
    } else if (TB_COUNT > input) {
        flowValueStr = QString::number(input / GB_COUNT + double(input % GB_COUNT) / GB_COUNT, 'd', prec) + " GB";
    } else {
        // 大于TB单位
        flowValueStr = QString::number(input / TB_COUNT + double(input % TB_COUNT) / TB_COUNT, 'd', prec) + " TB";
    }
    return flowValueStr;
}
