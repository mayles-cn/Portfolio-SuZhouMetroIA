#include "widgets/text_prompt_widget.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

TextPromptWidget::TextPromptWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "background-color: rgba(255,255,255,240);"
        "border: 1px solid rgba(36,66,112,140);"
        "border-radius: 10px;");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(8);

    inputEdit_ = new QLineEdit(this);
    inputEdit_->setPlaceholderText(
        QStringLiteral("\u8f93\u5165\u95ee\u9898\u540e\u6309\u56de\u8f66\u6216\u70b9\u51fb\u53d1\u9001\uff08F7 \u6253\u5f00/\u6536\u8d77\uff09"));
    inputEdit_->setStyleSheet(
        "QLineEdit {"
        "background: rgba(246,248,252,250);"
        "border: 1px solid rgba(35,69,118,120);"
        "border-radius: 7px;"
        "padding: 6px 10px;"
        "font: 12px 'Microsoft YaHei';"
        "}");

    sendButton_ = new QPushButton(QStringLiteral("\u53d1\u9001"), this);
    sendButton_->setCursor(Qt::PointingHandCursor);
    sendButton_->setFixedWidth(64);
    sendButton_->setStyleSheet(
        "QPushButton {"
        "background-color: rgba(32,92,182,220);"
        "color: white;"
        "border: none;"
        "border-radius: 7px;"
        "font: 700 12px 'Microsoft YaHei';"
        "padding: 6px 0;"
        "}"
        "QPushButton:hover { background-color: rgba(32,92,182,245); }"
        "QPushButton:pressed { background-color: rgba(24,74,149,245); }");

    layout->addWidget(inputEdit_, 1);
    layout->addWidget(sendButton_, 0);

    connect(sendButton_, &QPushButton::clicked, this, &TextPromptWidget::submitFromEditor);
    connect(inputEdit_, &QLineEdit::returnPressed, this, &TextPromptWidget::submitFromEditor);

    hide();
}

void TextPromptWidget::togglePanel()
{
    if (isVisible())
    {
        closePanel();
    }
    else
    {
        openPanel();
    }
}

void TextPromptWidget::openPanel()
{
    show();
    raise();
    if (inputEdit_ != nullptr)
    {
        inputEdit_->setFocus(Qt::ShortcutFocusReason);
        inputEdit_->selectAll();
    }
}

void TextPromptWidget::closePanel()
{
    hide();
}

bool TextPromptWidget::isPanelVisible() const
{
    return isVisible();
}

void TextPromptWidget::submitFromEditor()
{
    if (inputEdit_ == nullptr)
    {
        return;
    }

    const QString text = inputEdit_->text().trimmed();
    if (text.isEmpty())
    {
        return;
    }

    emit submitRequested(text);
    inputEdit_->clear();
}
