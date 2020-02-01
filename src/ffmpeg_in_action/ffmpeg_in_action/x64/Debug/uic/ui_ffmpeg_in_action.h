/********************************************************************************
** Form generated from reading UI file 'ffmpeg_in_action.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FFMPEG_IN_ACTION_H
#define UI_FFMPEG_IN_ACTION_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ffmpeg_in_actionClass
{
public:

    void setupUi(QWidget *ffmpeg_in_actionClass)
    {
        if (ffmpeg_in_actionClass->objectName().isEmpty())
            ffmpeg_in_actionClass->setObjectName(QString::fromUtf8("ffmpeg_in_actionClass"));
        ffmpeg_in_actionClass->resize(600, 400);

        retranslateUi(ffmpeg_in_actionClass);

        QMetaObject::connectSlotsByName(ffmpeg_in_actionClass);
    } // setupUi

    void retranslateUi(QWidget *ffmpeg_in_actionClass)
    {
        ffmpeg_in_actionClass->setWindowTitle(QApplication::translate("ffmpeg_in_actionClass", "ffmpeg_in_action", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ffmpeg_in_actionClass: public Ui_ffmpeg_in_actionClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FFMPEG_IN_ACTION_H
