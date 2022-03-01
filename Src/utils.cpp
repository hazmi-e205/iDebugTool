#include "utils.h"
#include <libimobiledevice-glue/utils.h>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QByteArray>
#include <QBitArray>
#include <QFileInfo>
#include <zip.h>
#include <dirent.h>
#include <stdio.h>

QJsonObject PlistToJsonObject(plist_t node)
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
    return jsonObject;
}

QJsonDocument PlistToJson(plist_t node)
{
    QJsonDocument jsonDoc;
    plist_type t = plist_get_node_type(node);
    switch (t) {
    case PLIST_ARRAY:
        {
            QJsonArray arr = PlistToJsonArray(node);
            jsonDoc.setArray(arr);
        }
        break;

    default:
        jsonDoc.setObject(PlistToJsonObject(node));
        break;
    }

    return jsonDoc;
}

QJsonArray PlistToJsonArray(plist_t node)
{
    QJsonArray arr;
    int i, count;
    plist_t subnode = nullptr;

    count = plist_array_get_size(node);
    for (i = 0; i < count; i++) {
        subnode = plist_array_get_item(node, i);
        arr.insert(0, PlistToJsonObject(subnode));
    }
    return arr;
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
    static bool m_newLine = false;
    switch(in){
    case '\0':
    {
        out = LogPacket(m_logTemp);
        m_logTemp = "";
        m_newLine = false;
        return true;
    }
    case '\n':
    {
        if (m_newLine) {
            m_logTemp += in;
        }
        m_newLine = true;
        break;
    }
    default:
        if (m_newLine) {
            m_logTemp += '\n';
            m_newLine = false;
        }
        m_logTemp += in;
        break;
    }
    return false;
}

int zip_get_contents(struct zip *zf, const char *filename, int locate_flags, char **buffer, uint32_t *len)
{
    struct zip_stat zs;
    struct zip_file *zfile;
    int zindex = zip_name_locate(zf, filename, locate_flags);

    *buffer = NULL;
    *len = 0;

    if (zindex < 0) {
        return -1;
    }

    zip_stat_init(&zs);

    if (zip_stat_index(zf, zindex, 0, &zs) != 0) {
        fprintf(stderr, "ERROR: zip_stat_index '%s' failed!\n", filename);
        return -2;
    }

    if (zs.size > 10485760) {
        fprintf(stderr, "ERROR: file '%s' is too large!\n", filename);
        return -3;
    }

    zfile = zip_fopen_index(zf, zindex, 0);
    if (!zfile) {
        fprintf(stderr, "ERROR: zip_fopen '%s' failed!\n", filename);
        return -4;
    }

    *buffer = (char*)malloc(zs.size);
    if (zs.size > LLONG_MAX || zip_fread(zfile, *buffer, zs.size) != (zip_int64_t)zs.size) {
        fprintf(stderr, "ERROR: zip_fread %llu bytes from '%s'\n", (uint64_t)zs.size, filename);
        free(*buffer);
        *buffer = NULL;
        zip_fclose(zfile);
        return -5;
    }
    *len = zs.size;
    zip_fclose(zfile);
    return 0;
}

int zip_get_app_directory(struct zip* zf, QString &path)
{
    int i = 0;
    int c = zip_get_num_files(zf);
    int len = 0;
    const char* name = NULL;

    /* look through all filenames in the archive */
    do {
        /* get filename at current index */
        name = zip_get_name(zf, i++, 0);
        if (name != NULL) {
            /* check if we have a "Payload/.../" name */
            len = strlen(name);
            if (!strncmp(name, "Payload/", 8) && (len > 8)) {
                /* skip hidden files */
                if (name[8] == '.')
                    continue;

                /* locate the second directory delimiter */
                const char* p = name + 8;
                do {
                    if (*p == '/') {
                        break;
                    }
                } while(p++ != NULL);

                /* try next entry if not found */
                if (p == NULL)
                    continue;

                /* copy filename */
                QFileInfo file_info(name);
                path = file_info.filePath().remove(file_info.fileName());
                break;
            }
        }
    } while(i < c);

    return 0;
}

int afc_upload_file(afc_client_t afc, QString &filename, QString &dstfn)
{
    FILE *f = NULL;
    uint64_t af = 0;
    char buf[1048576];

    f = fopen(filename.toUtf8().data(), "rb");
    if (!f) {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
        return -1;
    }

    if ((afc_file_open(afc, dstfn.toUtf8().data(), AFC_FOPEN_WRONLY, &af) != AFC_E_SUCCESS) || !af) {
        fclose(f);
        fprintf(stderr, "afc_file_open on '%s' failed!\n", dstfn.toUtf8().data());
        return -1;
    }

    size_t amount = 0;
    do {
        amount = fread(buf, 1, sizeof(buf), f);
        if (amount > 0) {
            uint32_t written, total = 0;
            while (total < amount) {
                written = 0;
                afc_error_t aerr = afc_file_write(afc, af, buf, amount, &written);
                if (aerr != AFC_E_SUCCESS) {
                    fprintf(stderr, "AFC Write error: %d\n", aerr);
                    break;
                }
                total += written;
            }
            if (total != amount) {
                fprintf(stderr, "Error: wrote only %u of %u\n", total, (uint32_t)amount);
                afc_file_close(afc, af);
                fclose(f);
                return -1;
            }
        }
    } while (amount > 0);

    afc_file_close(afc, af);
    fclose(f);

    return 0;
}

void afc_upload_dir(afc_client_t afc, QString &path, QString &afcpath)
{
    afc_make_directory(afc, afcpath.toUtf8().data());

    DIR *dir = opendir(path.toUtf8().data());
    if (dir) {
        struct dirent* ep;
        while ((ep = readdir(dir))) {
            if ((strcmp(ep->d_name, ".") == 0) || (strcmp(ep->d_name, "..") == 0)) {
                continue;
            }
            QString fpath = path + "/" + ep->d_name;
            QString apath = afcpath + "/" + ep->d_name;

            struct stat st;

            if ((stat(fpath.toUtf8().data(), &st) == 0) && S_ISDIR(st.st_mode)) {
                afc_upload_dir(afc, fpath, apath);
            } else {
                afc_upload_file(afc, fpath, apath);
            }
        }
        closedir(dir);
    }
}

void StringWithSpaces(QString &string, bool CapFirstOnly)
{
    QString temp;
    // Traverse the string
    for(int i=0; i < string.length(); i++)
    {
        // Convert to lowercase if its
        // an uppercase character
        if (string[i]>='A' && string[i]<='Z')
        {
            // add space before it
            // if its an uppercase character
            if (i != 0)
                temp += " ";

            // add the character
            if (CapFirstOnly && i != 0)
                temp += string[i].toLower();
            else
                temp += string[i];
        }

        // if lowercase character then just add
        else
            temp += string[i];
    }
    string = temp;
}
