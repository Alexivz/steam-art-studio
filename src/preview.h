#pragma once
#include <QImage>
#include <QProcess>
#include <QWidget>

namespace Studio {
class Preview : public QWidget {
    Q_OBJECT
public:
    explicit Preview(QWidget *parent = nullptr);
    ~Preview() override;
    void setFile(const QString &file, bool animated);
    void setBackgroundMode(int mode) { m_backgroundMode = mode; update(); }
    bool playing() const { return m_process.state() != QProcess::NotRunning; }
    bool animated() const { return m_animated; }
    QSize decodedSize() const { return m_image.size(); }
    void stop();
public slots:
    void togglePlayback();
signals:
    void playbackChanged(bool playing);
    void frameReady(const QSize &size);
    void error(const QString &message);
protected:
    void paintEvent(QPaintEvent *) override;
    void hideEvent(QHideEvent *) override;
private:
    QString m_file;
    QImage m_image;
    bool m_animated = false;
    int m_backgroundMode = 0;
    QProcess m_process;
    QByteArray m_buffer;
    QSize m_frameSize;
    qsizetype m_frameBytes = 0;
};
}
