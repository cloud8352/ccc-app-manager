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
