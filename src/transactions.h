#pragma once
#include "types.h"
#include "animation.h"
#include <QJsonArray>
#include <functional>

namespace Studio {
// Injectable I/O/checkpoints for deterministic synthetic fault tests. The app
// never exposes these hooks through arguments, settings or environment vars.
struct TransactionIo {
    std::function<bool(const QString &, const QByteArray &, QString *)> write;
    std::function<void(const QString &)> afterMutation;
};
class Transactions {
public:
    explicit Transactions(const QString &backupRoot, const TransactionIo &io = {});
    QString root() const { return m_root; }
    TransactionResult apply(const QVector<PendingChange> &changes, AnimationProfile profile = AnimationProfile::Original) const;
    TransactionResult restore(const QString &id) const;
    QJsonArray history() const;
    static bool validImage(const QString &path, bool *animated = nullptr, QString *error = nullptr);
    static bool validIcon(const QString &path, QString *error = nullptr);
    static QString destination(const Game &game, const QString &type);
    static QString recordDestination(const QJsonObject &record);
private:
    QString m_root;
    TransactionIo m_io;
    bool write(const QString &path, const QByteArray &bytes, QString *error) const;
    bool writeManifest(const QString &directory, const QJsonObject &manifest, QString *error = nullptr) const;
    TransactionResult restoreImpl(const QString &id, const QJsonObject &manifest) const;
};
}
