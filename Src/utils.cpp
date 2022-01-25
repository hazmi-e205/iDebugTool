#include "utils.h"
#include <libimobiledevice-glue/utils.h>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QByteArray>
#include <QBitArray>

QJsonDocument PlistToJson(plist_t node)
{
    QJsonObject jsonObject;
    plist_dict_iter it = nullptr;
    char* key = nullptr;
    plist_t subnode = nullptr;
    plist_dict_new_iter(node, &it);
    plist_dict_next_item(node, it, &key, &subnode);
    while (subnode)
    {
        jsonObject.insert(key, PlistNodeToJsonValue(subnode));
        free(key);
        key = nullptr;
        plist_dict_next_item(node, it, &key, &subnode);
    }
    free(it);

    QJsonDocument jsonDoc;
    jsonDoc.setObject(jsonObject);
    return jsonDoc;
}

QString Base64Encode(QString string)
{
    QByteArray ba;
    ba.append(string.toUtf8());
    return ba.toBase64();
}

QJsonValue PlistNodeToJsonValue(plist_t node)
{
    QJsonValue jsonValue;
    char *s = nullptr;
    char *data = nullptr;
    double d;
    uint8_t b;
    uint64_t u = 0;
    struct timeval tv = { 0, 0 };

    plist_type t;

    if (!node)
        return jsonValue;

    t = plist_get_node_type(node);

    switch (t) {
    case PLIST_BOOLEAN:
        plist_get_bool_val(node, &b);
        jsonValue = b ? true : false;
        break;

    case PLIST_UINT:
        plist_get_uint_val(node, &u);
        jsonValue = (double)u;
        break;

    case PLIST_REAL:
        plist_get_real_val(node, &d);
        jsonValue = d;
        break;

    case PLIST_STRING:
        plist_get_string_val(node, &s);
        jsonValue = s;
        free(s);
        break;

    case PLIST_KEY:
        plist_get_key_val(node, &s);
        //fprintf(stream, "%s: ", s);
        free(s);
        break;

    case PLIST_DATA:
        plist_get_data_val(node, &data, &u);
        if (u > 0) {
            QString str = Base64Encode(data);
            jsonValue = str;
        }
        break;

    case PLIST_DATE:
        plist_get_date_val(node, (int32_t*)&tv.tv_sec, (int32_t*)&tv.tv_usec);
        {
            time_t ti = (time_t)tv.tv_sec + MAC_EPOCH;
            struct tm *btime = localtime(&ti);
            if (btime) {
                s = (char*)malloc(24);
                memset(s, 0, 24);
                if (strftime(s, 24, "%Y-%m-%dT%H:%M:%SZ", btime) <= 0) {
                    free (s);
                    s = nullptr;
                }
            }
        }
        if (s) {
            jsonValue = s;
            free(s);
        }
        break;

    case PLIST_ARRAY:
        jsonValue = PlistArrayToJsonValue(node);
        break;

    case PLIST_DICT:
        jsonValue = PlistDictToJsonValue(node);
        break;

    default:
        break;
    }

    return jsonValue;
}

QJsonValue PlistDictToJsonValue(plist_t node)
{
    QJsonValue jsonValue;
    plist_dict_iter it = nullptr;
    char* key = nullptr;
    plist_t subnode = nullptr;
    plist_dict_new_iter(node, &it);
    plist_dict_next_item(node, it, &key, &subnode);
    while (subnode)
    {
        QJsonObject jsonObject;
        jsonObject.insert(key, PlistNodeToJsonValue(subnode));
        jsonValue = jsonObject;
        free(key);
        key = nullptr;
        plist_dict_next_item(node, it, &key, &subnode);
    }
    free(it);
    return jsonValue;
}

QJsonValue PlistArrayToJsonValue(plist_t node)
{
    QJsonValue jsonValue;
    int i, count;
    plist_t subnode = nullptr;

    count = plist_array_get_size(node);
    QJsonArray arr;
    for (i = 0; i < count; i++) {
        subnode = plist_array_get_item(node, i);
        arr.push_back(PlistNodeToJsonValue(subnode));
    }
    jsonValue = arr;
    return jsonValue;
}

bool ParseSystemLogs(char &in, LogPacket &out)
{
    static QString m_logTemp = "";
    switch(in){
    case '\0':
    {
        out = LogPacket(m_logTemp);
        m_logTemp = "";
        return true;
    }
    default:
        m_logTemp += in;
    }
    return false;
}
