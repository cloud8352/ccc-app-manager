#include "appmanagercommon.h"

//#include <DtkCores>
#include <QtCore>

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

#include <QHash>

static QHash<uint, QString> dict = {};

const char kDictFile[] = ":/dpinyin/resources/dpinyin.dict";

void InitDict() {
    if (!dict.isEmpty()) {
        return;
    }

    dict.reserve(25333);

    QFile file(kDictFile);

    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray content = file.readAll();

    file.close();

    QTextStream stream(&content, QIODevice::ReadOnly);

    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        const QStringList items = line.split(QChar(':'));

        if (items.size() == 2) {
            dict.insert(items[0].toInt(nullptr, 16), items[1]);
        }
    }
}

QString Chinese2Pinyin(const QString& words) {
    InitDict();

    QString result;

    for (int i = 0; i < words.length(); ++i) {
        const uint key = words.at(i).unicode();
        auto find_result = dict.find(key);

        if (find_result != dict.end()) {
            result.append(find_result.value());
        } else {
            result.append(words.at(i));
        }
    }

    return result;
}


AM::PinyinInfo AM::getPinYinInfoFromStr(const QString &words)
{
    PinyinInfo result;
    for (const QChar &singleChar : words) {
        QString singlePinyin = Chinese2Pinyin(singleChar);
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
        result.simpliyiedPinYin.append(singlePinyin.at(0));
    }

    return result;
}
