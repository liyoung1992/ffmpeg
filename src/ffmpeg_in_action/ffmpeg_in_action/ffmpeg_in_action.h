#pragma once

#include <QtWidgets/QWidget>
#include "ui_ffmpeg_in_action.h"

#include <stdio.h>
#include <string>
#include <memory>
#include <thread>
#include <iostream>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"

#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/time.h"
#include "SDL.h"
#include "libavutil/log.h"
#include <libavutil/timestamp.h>
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
	void init();
	//sdl播放器
	int SDL2Player();



	//保存网络流
	//网络包-demux》pes流-mux》本地存储

	int openInput(std::string input);
	int openOutput(std::string output);
	//static int interrupt_cb(void *ctx);

	int closeInput();
	int closeOutput();
	std::shared_ptr<AVPacket> readPacketFromSource();
	void av_packet_rescale_ts(AVPacket *pkt, AVRational src_tb, AVRational dst_tb);
	int writePacket(std::shared_ptr<AVPacket> packet);

	//裁剪文件
	void cutFile();

	AVFormatContext * getInputContext();
	AVFormatContext * getOutputContext();

	void doSave();

	// 抽取音频数据
	void getAudioData();

	//抽取视频数据
	void  getVideoData();

	//mp4toflv
	int mp4ToFlv();

	//cut_file
	int seekCutFile();

	//解码h264 decode
	int decode_video();
	int encode_video();

public slots:
	void on_sdl_play_btn_clicked();
	void on_save_stream_btn_clicked();
	void on_cutfile_btn_clicked();
	void on_getVideo_btn_clicked();
	void on_mp4flv_btn_clicked();
	void on_h264_decode_btn_clicked();
private:
	Ui::ffmpeg_in_actionClass  ui;

	AVFormatContext *inputContext = nullptr;
	AVFormatContext * outputContext;
	
};
