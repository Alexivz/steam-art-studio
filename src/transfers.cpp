#include "transfers.h"
#include <QSize>

namespace Studio {
namespace {
QString transferBytes(qint64 bytes) {
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024., 'f', 1) + " KiB";
    return QString::number(bytes / (1024. * 1024.), 'f', 1) + " MiB";
}
}
TransferModel::TransferModel(Network *network, QObject *parent) : QAbstractListModel(parent), m_network(network) {
    connect(network, &Network::progress, this, [this](quint64 token, qint64 received, qint64 total) {
        const int row = find(token); if (row < 0) return;
        auto &entry = m_entries[row]; entry.received = received; entry.total = total; entry.state = Downloading; notify(row);
    });
    connect(network, &Network::ready, this, [this](quint64 token, const QString &file, bool cached) {
        const int row = find(token); if (row < 0) return;
        m_entries[row].state = Complete; const ArtTask task = m_entries[row].task; notify(row); emit completed(task, file, cached);
    });
    connect(network, &Network::failed, this, [this](quint64 token, const QString &message, int) {
        const int row = find(token); if (row < 0) return;
        m_entries[row].state = Failed; m_entries[row].error = message; notify(row);
        emit error(message);
    });
    connect(network, &Network::cancelled, this, [this](quint64 token) {
        const int row = find(token); if (row < 0) return;
        m_entries[row].state = Cancelled; notify(row);
    });
}
int TransferModel::find(quint64 token) const {
    for (int i = 0; i < m_entries.size(); ++i) if (m_entries[i].token == token && (m_entries[i].state == Waiting || m_entries[i].state == Downloading)) return i;
    return -1;
}
int TransferModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_entries.size(); }
QVariant TransferModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) return {};
    const auto &entry = m_entries[index.row()];
    if (role == StateRole) return entry.state;
    if (role == ReceivedRole) return entry.received;
    if (role == TotalRole) return entry.total;
    if (role == ErrorRole || role == Qt::ToolTipRole) return entry.error;
    if (role == Qt::SizeHintRole) return QSize(240, 80);
    if (role == Qt::DisplayRole) {
        const QStringList states = {"En espera", "Descargando", "Descargada", "Error", "Cancelada"};
        QString progress = transferBytes(entry.received);
        if (entry.total > 0) progress = QString("%1% · %2 / %3").arg(qBound<qint64>(0LL, entry.received * 100 / entry.total, 100LL)).arg(transferBytes(entry.received), transferBytes(entry.total));
        return entry.title + "\n" + states[entry.state] + (entry.state == Downloading ? " · " + progress : entry.state == Failed ? " · " + (entry.received > 0 ? progress + " · " : QString()) + entry.error : QString());
    }
    return {};
}
void TransferModel::notify(int row) { emit dataChanged(index(row), index(row)); emit changed(); }
bool TransferModel::enqueue(const QUrl &url, const QString &title, const ArtTask &task) {
    if (m_entries.size() >= 100) clearFinished();
    if (m_entries.size() >= 100) { emit error("Hay 100 descargas pendientes. Cancela alguna antes de añadir más."); return false; }
    Entry entry; entry.url = url; entry.title = title; entry.task = task; entry.token = m_network->fetch(url, "original");
    beginInsertRows({}, m_entries.size(), m_entries.size()); m_entries.append(entry); endInsertRows(); emit changed(); return true;
}
void TransferModel::cancel(int row) {
    if (row >= 0 && row < m_entries.size() && (m_entries[row].state == Waiting || m_entries[row].state == Downloading)) m_network->cancel(m_entries[row].token);
}
void TransferModel::retry(int row) {
    if (row < 0 || row >= m_entries.size() || m_entries[row].state == Waiting || m_entries[row].state == Downloading) return;
    auto &entry = m_entries[row]; entry.error.clear(); entry.received = 0; entry.total = -1; entry.state = Waiting;
    entry.token = m_network->fetch(entry.url, "original"); notify(row);
}
void TransferModel::clearFinished() {
    for (int i = m_entries.size() - 1; i >= 0; --i) if (m_entries[i].state != Waiting && m_entries[i].state != Downloading) { beginRemoveRows({}, i, i); m_entries.removeAt(i); endRemoveRows(); }
    emit changed();
}
int TransferModel::activeCount() const { int active = 0; for (const auto &entry : m_entries) active += entry.state == Waiting || entry.state == Downloading; return active; }
}
