#include "services/app_config_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace szmetro
{
namespace
{
QString resolveConfigPath(const QString& fileName)
{
    const QDir currentDir = QDir::current();
    const QDir appDir = QDir(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
        currentDir.filePath(QStringLiteral("config/%1").arg(fileName)),
        appDir.filePath(QStringLiteral("../config/%1").arg(fileName)),
        appDir.filePath(QStringLiteral("../../config/%1").arg(fileName)),
        appDir.filePath(QStringLiteral("config/%1").arg(fileName)),
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
} // namespace

AppConfig AppConfigService::load()
{
    AppConfig config;

    const QString apiKeyPath = resolveConfigPath(QStringLiteral("api_keys.json"));
    config.configPath = apiKeyPath;
    if (!apiKeyPath.isEmpty())
    {
        QFile file(apiKeyPath);
        if (file.open(QIODevice::ReadOnly))
        {
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error == QJsonParseError::NoError && doc.isObject())
            {
                const QJsonObject root = doc.object();
                const QJsonObject qwen = root.value(QStringLiteral("Qwen3")).toObject();
                config.qwenApiKey = qwen.value(QStringLiteral("LLM_service_key")).toString().trimmed();
                const QString model = qwen.value(QStringLiteral("model")).toString().trimmed();
                if (!model.isEmpty())
                {
                    config.llmModel = model;
                }
            }
        }
    }

    const QString rulePath = resolveConfigPath(QStringLiteral("assistant_rules.json"));
    if (!rulePath.isEmpty())
    {
        QFile file(rulePath);
        if (file.open(QIODevice::ReadOnly))
        {
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error == QJsonParseError::NoError && doc.isObject())
            {
                const QJsonObject root = doc.object();
                const QString llmModel = root.value(QStringLiteral("llm_model")).toString().trimmed();
                if (!llmModel.isEmpty())
                {
                    config.llmModel = llmModel;
                }
                const QString asrModel =
                    root.value(QStringLiteral("asr_realtime_model")).toString().trimmed();
                if (!asrModel.isEmpty())
                {
                    config.asrRealtimeModel = asrModel;
                }

                const QJsonArray rules = root.value(QStringLiteral("response_rules")).toArray();
                QStringList loadedRules;
                loadedRules.reserve(rules.size());
                for (const QJsonValue& value : rules)
                {
                    if (!value.isString())
                    {
                        continue;
                    }
                    const QString text = value.toString().trimmed();
                    if (!text.isEmpty())
                    {
                        loadedRules.push_back(text);
                    }
                }
                if (!loadedRules.isEmpty())
                {
                    config.assistantRules = loadedRules;
                }
            }
        }
    }

    return config;
}
} // namespace szmetro
