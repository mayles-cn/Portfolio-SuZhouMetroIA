#pragma once

#include <QString>

namespace szmetro
{
class UiStyleService
{
public:
    // Get style by key. Return fallback when key is missing.
    static QString style(const QString& key, const QString& fallback = QString());

    // Return resolved config file path. May be empty.
    static QString styleConfigPath();

    // Clear cache; styles will be reloaded lazily on next style() call.
    static void reload();
};
} // namespace szmetro
