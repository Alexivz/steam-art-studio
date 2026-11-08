#include "valvekv.h"
#include <QtEndian>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <QSet>

namespace Studio {
namespace {
class Reader {
public:
    const QByteArray &data;
    qsizetype pos = 0;
    explicit Reader(const QByteArray &bytes) : data(bytes) {}
    void require(qsizetype n) const { if (n < 0 || pos < 0 || n > data.size() - pos) throw std::runtime_error("Archivo VDF incompleto"); }
    quint8 byte() { require(1); return static_cast<quint8>(data.at(pos++)); }
    quint16 u16() { require(2); const auto n = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + pos)); pos += 2; return n; }
    quint32 u32() { require(4); auto n = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + pos)); pos += 4; return n; }
    quint64 u64() { require(8); auto n = qFromLittleEndian<quint64>(reinterpret_cast<const uchar *>(data.constData() + pos)); pos += 8; return n; }
    QString cstring() {
        const auto end = data.indexOf('\0', pos);
        if (end < 0 || end - pos > 4 * 1024 * 1024) throw std::runtime_error("Cadena VDF inválida");
        auto result = QString::fromUtf8(data.constData() + pos, end - pos); pos = end + 1; return result;
    }
    void skip(quint8 type, int depth = 0) {
        if (depth > 64) throw std::runtime_error("VDF demasiado anidado");
        switch (type) {
        case 0:
            while (true) { const auto child = byte(); if (child == 8 || child == 11) break; cstring(); skip(child, depth + 1); }
            break;
        case 1: cstring(); break;
        case 2: case 3: case 4: case 6: u32(); break;
        case 7: case 10: u64(); break;
        case 5: { const quint32 n = u16(); require(n * 2); pos += n * 2; break; }
        default: throw std::runtime_error("Tipo binario VDF no compatible: no se modificará el archivo");
        }
    }
    QVariantMap object(const QStringList &keys, int depth = 0) {
        if (depth > 64) throw std::runtime_error("VDF demasiado anidado");
        QVariantMap result;
        while (pos < data.size()) {
            const quint8 type = byte();
            if (type == 8 || type == 11) return result;
            QString key;
            if (keys.isEmpty()) key = cstring();
            else { const quint32 index = u32(); if (index >= static_cast<quint32>(keys.size())) throw std::runtime_error("Índice VDF inválido"); key = keys.at(index); }
            switch (type) {
            case 0: result[key] = object(keys, depth + 1); break;
            case 1: result[key] = cstring(); break;
            case 2: case 4: case 6: result[key] = u32(); break;
            case 3: { const quint32 bits = u32(); float n = 0; std::memcpy(&n, &bits, sizeof(n)); result[key] = n; break; }
            case 7: result[key] = QVariant::fromValue(u64()); break;
            case 10: result[key] = QVariant::fromValue(static_cast<qint64>(u64())); break;
            case 5: {
                const quint32 length = u16();
                if (length > 32768) throw std::runtime_error("Cadena UTF-16 demasiado larga");
                require(static_cast<qsizetype>(length) * 2);
                QString text;
                for (quint32 i = 0; i < length; ++i) { quint16 c = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + pos)); pos += 2; if (c) text.append(QChar(c)); }
                result[key] = text; break;
            }
            default: throw std::runtime_error("Tipo binario VDF no compatible");
            }
            if (result.size() > 1000000) throw std::runtime_error("Objeto VDF demasiado grande");
        }
        throw std::runtime_error("Objeto VDF sin cerrar");
    }
};
class TextReader {
public:
    QString data;
    qsizetype pos = 0;
    explicit TextReader(const QByteArray &bytes) : data(QString::fromUtf8(bytes)) { if (data.startsWith(QChar(0xfeff))) ++pos; }
    void whitespace() {
        while (pos < data.size()) {
            if (data.at(pos).isSpace()) { ++pos; continue; }
            if (data.mid(pos, 2) == "//") { auto end = data.indexOf('\n', pos); pos = end < 0 ? data.size() : end + 1; continue; }
            break;
        }
    }
    QString token() {
        whitespace();
        if (pos >= data.size()) return {};
        if (data.at(pos) == '"') {
            ++pos; QString result;
            while (pos < data.size()) {
                QChar c = data.at(pos++);
                if (c == '"') return result;
                if (c == '\\' && pos < data.size()) {
                    const auto next = data.at(pos);
                    if (next == '"' || next == '\\') { result += next; ++pos; continue; }
                }
                result += c;
            }
            throw std::runtime_error("Cadena de texto VDF sin cerrar");
        }
        if (data.at(pos) == '{' || data.at(pos) == '}') return QString(data.at(pos++));
        const auto start = pos;
        while (pos < data.size() && !data.at(pos).isSpace() && data.at(pos) != '{' && data.at(pos) != '}') ++pos;
        return data.mid(start, pos - start);
    }
    QVariantMap object(int depth = 0, bool nested = false) {
        if (depth > 64) throw std::runtime_error("VDF demasiado anidado");
        QVariantMap result;
        while (true) {
            whitespace();
            if (pos >= data.size()) { if (nested) throw std::runtime_error("Objeto de texto VDF sin cerrar"); return result; }
            QString key = token();
            if (key == "}") { if (!nested) throw std::runtime_error("Llave VDF inesperada"); return result; }
            if (key == "{") throw std::runtime_error("Clave VDF inválida");
            QString val = token();
            if (val == "{") result[key] = object(depth + 1, true);
            else if (val == "}") throw std::runtime_error("Falta un valor VDF");
            else result[key] = val;
        }
    }
};
}
QVariantMap ValveKv::text(const QByteArray &data, QString *error) {
    try { if (data.size() > 64 * 1024 * 1024) throw std::runtime_error("VDF demasiado grande"); TextReader r(data); return r.object(); }
    catch (const std::exception &e) { if (error) *error = QString::fromUtf8(e.what()); return {}; }
}
QVariantMap ValveKv::binary(const QByteArray &data, const QStringList &keys, QString *error) {
    try { Reader r(data); return r.object(keys); }
    catch (const std::exception &e) { if (error) *error = QString::fromUtf8(e.what()); return {}; }
}
QByteArray ValveKv::shortcutIcons(const QByteArray &data, const QMap<quint32, QString> &icons, QString *error) {
    struct Patch { qsizetype start, length; QByteArray value; };
    QVector<Patch> patches;
    try {
        if (data.isEmpty() || data.size() > 16 * 1024 * 1024 || icons.isEmpty()) throw std::runtime_error("Lista de accesos no válida");
        for (const auto &value : icons) if (value.isEmpty() || value.contains(QChar::Null) || value.size() > 32768) throw std::runtime_error("Ruta de icono no válida");
        Reader r(data); bool foundRoot = false, ended = false; QSet<quint32> found;
        while (r.pos < data.size()) {
            const auto type = r.byte();
            if (type == 8 || type == 11) { ended = true; break; }
            const auto key = r.cstring();
            if (key.compare("shortcuts", Qt::CaseInsensitive) != 0) { r.skip(type); continue; }
            if (foundRoot || type != 0) throw std::runtime_error("Lista de accesos ambigua");
            foundRoot = true;
            while (true) {
                const auto entryType = r.byte(); if (entryType == 8 || entryType == 11) break;
                r.cstring();
                if (entryType != 0) throw std::runtime_error("Entrada de acceso no válida");
                quint32 id = 0; bool hasId = false, hasIcon = false;
                qsizetype iconStart = 0, iconEnd = 0, end = 0;
                while (true) {
                    const auto fieldType = r.byte();
                    if (fieldType == 8 || fieldType == 11) { end = r.pos - 1; break; }
                    const auto field = r.cstring();
                    if (field.compare("appid", Qt::CaseInsensitive) == 0) {
                        if (hasId || fieldType != 2) throw std::runtime_error("AppID de acceso ambiguo");
                        hasId = true; id = r.u32() | 0x80000000U;
                    } else if (field.compare("icon", Qt::CaseInsensitive) == 0) {
                        if (hasIcon || fieldType != 1) throw std::runtime_error("Campo de icono ambiguo");
                        hasIcon = true; iconStart = r.pos; r.cstring(); iconEnd = r.pos;
                    } else r.skip(fieldType, 2);
                }
                if (!hasId || !icons.contains(id)) continue;
                if (found.contains(id)) throw std::runtime_error("Hay accesos duplicados con el mismo AppID");
                found.insert(id);
                QByteArray value = icons.value(id).toUtf8(); value.append('\0');
                if (hasIcon) patches.append({iconStart, iconEnd - iconStart, value});
                else patches.append({end, 0, QByteArray("\x01" "icon\0", 6) + value});
            }
        }
        if (!foundRoot || !ended || r.pos != data.size() || found.size() != icons.size()) throw std::runtime_error("No se encuentran todos los accesos o el VDF está incompleto");
        std::sort(patches.begin(), patches.end(), [](const Patch &a, const Patch &b) { return a.start > b.start; });
        QByteArray output = data;
        for (const auto &patch : patches) output.replace(patch.start, patch.length, patch.value);
        return output;
    } catch (const std::exception &e) { if (error) *error = QString::fromUtf8(e.what()); return {}; }
}
QMap<quint32, QVariantMap> ValveKv::appInfo(const QByteArray &data, QString *error) {
    QMap<quint32, QVariantMap> result;
    try {
        if (data.size() > 256 * 1024 * 1024) throw std::runtime_error("appinfo.vdf demasiado grande");
        Reader r(data); const auto magic = r.u32(); r.u32();
        if (magic < 0x07564427 || magic > 0x07564429) throw std::runtime_error("Versión de appinfo.vdf no compatible");
        QStringList keys;
        if (magic >= 0x07564429) {
            const quint64 table = r.u64(); if (table >= static_cast<quint64>(data.size())) throw std::runtime_error("Tabla appinfo inválida");
            const auto start = r.pos; r.pos = static_cast<qsizetype>(table); const auto count = r.u32();
            if (count > 1000000) throw std::runtime_error("Tabla appinfo demasiado grande");
            keys.reserve(static_cast<int>(count)); for (quint32 i = 0; i < count; ++i) keys.append(r.cstring()); r.pos = start;
        }
        while (r.pos < data.size()) {
            const auto id = r.u32(); if (!id) break;
            const auto size = r.u32(); const auto start = r.pos;
            const quint32 metadata = magic >= 0x07564428 ? 60 : 40;
            if (size < metadata || static_cast<quint64>(start) + size > static_cast<quint64>(data.size())) throw std::runtime_error("Entrada appinfo truncada");
            const QByteArray entry = data.mid(start + metadata, size - metadata);
            Reader er(entry); QVariantMap object = er.object(keys);
            auto info = child(object, "appinfo"); if (info.isEmpty()) info = object;
            result[id] = info; r.pos = start + size;
        }
    } catch (const std::exception &e) { if (error) *error = QString::fromUtf8(e.what()); return {}; }
    return result;
}
QVariant ValveKv::value(const QVariantMap &map, const QString &key) {
    auto it = map.constFind(key); if (it != map.cend()) return it.value();
    for (auto i = map.cbegin(); i != map.cend(); ++i) if (i.key().compare(key, Qt::CaseInsensitive) == 0) return i.value();
    return {};
}
QVariantMap ValveKv::child(const QVariantMap &map, const QString &key) { return value(map, key).toMap(); }
}
