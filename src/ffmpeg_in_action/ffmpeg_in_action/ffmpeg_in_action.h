#pragma once

#include <QtWidgets/QWidget>
#include "ui_ffmpeg_in_action.h"

#include <stdio.h>


#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "SDL.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
};
#endif
#endif

//Output YUV420P data as a file 
#define OUTPUT_YUV420P 0


class ffmpeg_in_action : public QWidget
{
	Q_OBJECT

public:
	ffmpeg_in_action(QWidget *parent = Q_NULLPTR);
	//sdl播放器
	int SDL2Player();
	//保存网络流
	//网络包-demux》pes流-mux》本地存储
	void saveStream();
	int openInput(std::string input);
	int openOutput(std::string output);
public slots:
	void on_sdl_play_btn_clicked();

private:
	Ui::ffmpeg_in_actionClass ui;
};
