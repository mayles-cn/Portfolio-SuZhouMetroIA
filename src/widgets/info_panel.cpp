#include "widgets/info_panel.h"

#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

InfoPanel::InfoPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    titleLabel_ = new QLabel(this);
    titleLabel_->setStyleSheet("font-size: 24px; font-weight: 600;");
    layout->addWidget(titleLabel_);

    contentView_ = new QTextEdit(this);
    contentView_->setReadOnly(true);
    layout->addWidget(contentView_, 1);
}

void InfoPanel::setTitle(const QString& title)
{
    titleLabel_->setText(title);
}

void InfoPanel::setContent(const QString& content)
{
    contentView_->setPlainText(content);
}
