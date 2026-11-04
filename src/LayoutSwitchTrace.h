#pragma once
// Local opt-in diagnostic. No output or clock reads unless explicitly enabled.
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>
#include <QThread>
#include <QTimer>
#include <chrono>
#include <utility>

namespace layout_trace {
inline const QString &path()
{
    static const QString value = qEnvironmentVariable("WIZARD_LAYOUT_TRACE_PATH");
    return value;
}
inline qint64 now()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline quint64 currentId()
{
    return path().isEmpty() || !QCoreApplication::instance() ? 0
        : QCoreApplication::instance()->property("wizardLayoutTraceId").toULongLong();
}
// GUI-thread spans are buffered through one application-owned sink shared by
// Wizard and the QtADS dylib. Flush only after returning to the event loop.
class BufferedSink final : public QObject
{
public:
    explicit BufferedSink(QObject *parent) : QObject(parent)
    {
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, this, [this] { flush(); });
    }
    ~BufferedSink() override { flush(); }
    void append(const QByteArray &line)
    {
        buffer += line;
        if (!timer.isActive())
            timer.start(0);
    }
private:
    void flush()
    {
        if (buffer.isEmpty())
            return;
        QFile file(path());
        if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            file.write(buffer);
            buffer.clear();
        }
    }
    QByteArray buffer;
    QTimer timer;
};
inline void writeLine(const QByteArray &line)
{
    auto *app = QCoreApplication::instance();
    if (app && QThread::currentThread() == app->thread()) {
        auto *sink = static_cast<BufferedSink *>(
            app->property("wizardLayoutTraceBufferedSink").value<QObject *>());
        if (!sink) {
            sink = new BufferedSink(app);
            app->setProperty("wizardLayoutTraceBufferedSink",
                             QVariant::fromValue(static_cast<QObject *>(sink)));
        }
        sink->append(line);
    } else {
        // Rare render-thread frame markers do not access GUI-owned state.
        QFile file(path());
        if (file.open(QIODevice::WriteOnly | QIODevice::Append))
            file.write(line);
    }
}
inline void record(quint64 id, const char *kind, const QString &stage,
                   qint64 timestamp, double durationMs = -1,
                   QJsonObject details = {})
{
    if (!id || path().isEmpty())
        return;
    details.insert("switch_id", double(id));
    details.insert("pid", double(QCoreApplication::applicationPid()));
    details.insert("kind", QString::fromLatin1(kind));
    details.insert("stage", stage);
    // String keeps nanosecond precision when consumed by JavaScript.
    details.insert("start_ns", QString::number(timestamp));
    if (durationMs >= 0)
        details.insert("duration_ms", durationMs);
    writeLine(QJsonDocument(details).toJson(QJsonDocument::Compact) + '\n');
}
class Span final
{
public:
    explicit Span(const char *stage, QJsonObject details = {})
        : m_id(currentId()), m_stage(stage), m_start(m_id ? now() : 0),
          m_details(std::move(details)) {}
    ~Span()
    {
        if (m_id)
            record(m_id, "span", QString::fromLatin1(m_stage), m_start,
                   double(now() - m_start) / 1e6, m_details);
    }
    Span(const Span &) = delete;
    Span &operator=(const Span &) = delete;
private:
    quint64 m_id;
    const char *m_stage;
    qint64 m_start;
    QJsonObject m_details;
};
template <typename F> decltype(auto) timed(const char *stage, F &&function)
{
    const Span span(stage);
    return std::forward<F>(function)();
}
} // namespace layout_trace
