#include "transactions.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <unistd.h>

using namespace Studio;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv); const auto args = app.arguments();
    const QString root = QFileInfo(args.value(1)).canonicalFilePath(); QFile marker(root + "/.synthetic-fixture");
    // This worker cannot be pointed at a real Steam installation.
    if (!root.startsWith("/tmp/opencode/studio-fault-") || !marker.open(QIODevice::ReadOnly) || marker.readAll() != "synthetic-only\n") return 3;
    TransactionIo io; io.afterMutation = [](const QString &) { ::_exit(86); };
    Transactions transactions(root + "/backups", io);
    if (args.value(2) == "restore") return transactions.restore(args.value(3)).ok ? 0 : 4;
    QVector<PendingChange> changes;
    for (quint32 id : {100U, 200U}) { Game game; game.appId = id; game.name = "Synthetic"; game.steamRoot = root + "/steam"; game.userId = "42"; changes.append({game, "capsule", root + "/new.png", "Synthetic", false}); }
    return transactions.apply(changes).ok ? 0 : 4;
}
