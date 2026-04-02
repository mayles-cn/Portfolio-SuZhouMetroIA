#pragma once

#include <QWidget>

class QLineEdit;
class QPushButton;

class TextPromptWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TextPromptWidget(QWidget* parent = nullptr);

    void togglePanel();
    void openPanel();
    void closePanel();
    bool isPanelVisible() const;

signals:
    void submitRequested(const QString& text);

private:
    void submitFromEditor();

    QLineEdit* inputEdit_ = nullptr;
    QPushButton* sendButton_ = nullptr;
};

