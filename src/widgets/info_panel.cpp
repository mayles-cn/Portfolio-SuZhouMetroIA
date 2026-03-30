#include "widgets/info_panel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

InfoPanel::InfoPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    headerBar_ = new QWidget(this);
    headerBar_->setFixedHeight(52);
    auto* headerLayout = new QHBoxLayout(headerBar_);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);

    titleLabel_ = new QLabel(headerBar_);
    titleLabel_->setStyleSheet("font-size: 24px; font-weight: 600;");
    headerLayout->addWidget(titleLabel_);
    headerLayout->addStretch();

    exitButton_ = new QPushButton(QStringLiteral("Exit"), headerBar_);
    exitButton_->setFixedSize(88, 32);
    exitButton_->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(exitButton_);

    layout->addWidget(headerBar_);

    contentView_ = new QTextEdit(this);
    contentView_->setReadOnly(true);
    layout->addWidget(contentView_, 1);

    connect(exitButton_, &QPushButton::clicked, this, &InfoPanel::exitClicked);
}

void InfoPanel::setTitle(const QString& title)
{
    titleLabel_->setText(title);
}

void InfoPanel::setContent(const QString& content)
{
    contentView_->setPlainText(content);
}
