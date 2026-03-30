#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTextEdit;

class InfoPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit InfoPanel(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setContent(const QString& content);

signals:
    void exitClicked();

private:
    QWidget* headerBar_ = nullptr;
    QPushButton* exitButton_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QTextEdit* contentView_ = nullptr;
};
