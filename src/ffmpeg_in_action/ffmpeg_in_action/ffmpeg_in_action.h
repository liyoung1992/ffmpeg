#pragma once

#include <QtWidgets/QWidget>
#include "ui_ffmpeg_in_action.h"

class ffmpeg_in_action : public QWidget
{
	Q_OBJECT

public:
	ffmpeg_in_action(QWidget *parent = Q_NULLPTR);

private:
	Ui::ffmpeg_in_actionClass ui;
};
