#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQueue>
#include <QTimer>
#include <cstring>

struct FakeResponse {
    int status = 200, delay = 1, chunk = 65536, abortDelay = 0;
    QByteArray body = "synthetic-image", mime = "image/png", retryAfter;
    QUrl redirect;
    bool stall = false, metadata = true;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    qint64 stallAfterBytes = 0;
};
class FakeReply : public QNetworkReply {
public:
    FakeReply(const QNetworkRequest &request, const FakeResponse &response, QObject *parent) : QNetworkReply(parent), m_response(response) {
        setRequest(request); setUrl(request.url()); setOperation(QNetworkAccessManager::GetOperation); open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.status); setHeader(QNetworkRequest::ContentTypeHeader, response.mime);
        if (response.redirect.isValid()) setAttribute(QNetworkRequest::RedirectionTargetAttribute, response.redirect);
        if (!response.retryAfter.isEmpty()) setRawHeader("Retry-After", response.retryAfter);
        QTimer::singleShot(response.delay, this, [this] { if (isFinished()) return; if (m_response.metadata) emit metaDataChanged(); if (!m_response.stall) tick(); });
    }
    void abort() override {
        if (isFinished()) return;
        if (m_response.abortDelay > 0) {
            const int delay = m_response.abortDelay; m_response.abortDelay = 0; m_response.stall = true;
            QTimer::singleShot(delay, this, [this] { abort(); }); return;
        }
        setError(QNetworkReply::OperationCanceledError, "Synthetic cancellation"); setFinished(true); emit errorOccurred(error()); emit finished();
    }
    qint64 bytesAvailable() const override { return m_buffer.size() + QIODevice::bytesAvailable(); }
    bool isSequential() const override { return true; }
protected:
    qint64 readData(char *target, qint64 maximum) override {
        const auto count = qMin<qint64>(maximum, m_buffer.size());
        if (count) { std::memcpy(target, m_buffer.constData(), count); m_buffer.remove(0, count); return count; }
        return isFinished() ? -1 : 0;
    }
private:
    FakeResponse m_response; QByteArray m_buffer; qsizetype m_position = 0;
    void tick() {
        if (isFinished()) return;
        const auto bytes = m_response.body.mid(m_position, m_response.chunk); m_position += bytes.size(); m_buffer.append(bytes);
        if (!bytes.isEmpty()) emit readyRead();
        if (isFinished()) return;
        emit downloadProgress(m_position, m_response.body.size());
        if (m_response.stallAfterBytes > 0 && m_position >= m_response.stallAfterBytes && m_position < m_response.body.size()) return;
        if (m_position < m_response.body.size()) QTimer::singleShot(m_response.delay, this, [this] { tick(); });
        else { if (m_response.error != NoError) { setError(m_response.error, "Synthetic transport error"); emit errorOccurred(error()); } setFinished(true); emit finished(); }
    }
};
class FakeNetwork : public QNetworkAccessManager {
public:
    QQueue<FakeResponse> responses;
    QVector<QNetworkRequest> requests;
    int active = 0, images = 0, peak = 0, peakImages = 0;
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override {
        requests.append(request); const bool image = !request.url().path().startsWith("/api/");
        ++active; images += image; peak = qMax(peak, active); peakImages = qMax(peakImages, images);
        FakeResponse response;
        if (responses.isEmpty()) { response.status = 500; response.body = "Unplanned synthetic request"; }
        else response = responses.dequeue();
        auto *reply = new FakeReply(request, response, this);
        connect(reply, &QNetworkReply::finished, this, [this, image] { --active; images -= image; }); return reply;
    }
};
