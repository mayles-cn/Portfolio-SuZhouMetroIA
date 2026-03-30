#pragma once

#include <QWidget>

class QLabel;
class QTextEdit;

class InfoPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit InfoPanel(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setContent(const QString& content);

private:
    QLabel* titleLabel_ = nullptr;
    QTextEdit* contentView_ = nullptr;
};
