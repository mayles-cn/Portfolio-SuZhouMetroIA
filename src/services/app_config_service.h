#pragma once

#include <QString>
#include <QStringList>

namespace szmetro
{
struct AppConfig
{
    QString configPath;
    QString qwenApiKey;
    QString llmModel = QStringLiteral("qwen-plus");
    QString asrRealtimeModel = QStringLiteral("qwen3-asr-flash-realtime");
    QStringList assistantRules = {
        QStringLiteral("Always answer in Chinese with concise and actionable wording."),
        QStringLiteral("Focus on Suzhou metro: lines, stations, fare, and transfer."),
        QStringLiteral("Use tools first for fare, line, and station ownership questions."),
        QStringLiteral("If information is missing, state gaps before suggestions.")};
};

class AppConfigService
{
public:
    static AppConfig load();
};
} // namespace szmetro
