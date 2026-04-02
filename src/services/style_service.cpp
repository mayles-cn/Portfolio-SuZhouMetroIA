#include "services/style_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>

namespace szmetro
{
namespace
{
struct StyleCache
{
    bool loaded = false;
    QString configPath;
    QHash<QString, QString> values;
};

StyleCache& cache()
{
    static StyleCache c;
    return c;
}

QString resolveStyleConfigPath()
{
    const QDir currentDir = QDir::current();
    const QDir appDir = QDir(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
        currentDir.filePath(QStringLiteral("config/ui_styles.json")),
        appDir.filePath(QStringLiteral("../config/ui_styles.json")),
        appDir.filePath(QStringLiteral("../../config/ui_styles.json")),
        appDir.filePath(QStringLiteral("config/ui_styles.json")),
    };

    for (const QString& candidate : candidates)
    {
        const QFileInfo info(QDir::cleanPath(candidate));
        if (info.exists() && info.isFile())
        {
            return info.absoluteFilePath();
        }
    }
    return {};
}

void ensureLoaded(StyleCache* c)
{
    if (c == nullptr || c->loaded)
    {
        return;
    }
    c->loaded = true;
    c->values.clear();
    c->configPath = resolveStyleConfigPath();
    if (c->configPath.isEmpty())
    {
        return;
    }

    QFile file(c->configPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return;
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
    {
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonObject styles = root.value(QStringLiteral("styles")).toObject();
    for (auto it = styles.constBegin(); it != styles.constEnd(); ++it)
    {
        if (!it.value().isString())
        {
            continue;
        }
        const QString key = it.key().trimmed();
        const QString value = it.value().toString();
        if (key.isEmpty() || value.isEmpty())
        {
            continue;
        }
        c->values.insert(key, value);
    }
}
} // namespace

QString UiStyleService::style(const QString& key, const QString& fallback)
{
    StyleCache& c = cache();
    ensureLoaded(&c);
    const QString trimmedKey = key.trimmed();
    if (trimmedKey.isEmpty())
    {
        return fallback;
    }

    const auto it = c.values.constFind(trimmedKey);
    if (it == c.values.cend())
    {
        return fallback;
    }
    return it.value();
}

QString UiStyleService::styleConfigPath()
{
    StyleCache& c = cache();
    ensureLoaded(&c);
    return c.configPath;
}

void UiStyleService::reload()
{
    StyleCache& c = cache();
    c.loaded = false;
    c.values.clear();
    c.configPath.clear();
}
} // namespace szmetro


