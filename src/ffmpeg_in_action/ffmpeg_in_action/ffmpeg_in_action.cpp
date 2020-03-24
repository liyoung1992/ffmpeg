#include "ffmpeg_in_action.h"

ffmpeg_in_action::ffmpeg_in_action(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	init();
}

/*
	初始化ffmpeg相关配置
*/
void ffmpeg_in_action::init()
{
	av_register_all();
	avfilter_register_all();
	avformat_network_init();
	av_log_set_level(AV_LOG_ERROR);
}

int ffmpeg_in_action::SDL2Player() {
	AVFormatContext	*pFormatCtx;
	int				i, videoindex;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	AVFrame	*pFrame, *pFrameYUV;
	unsigned char *out_buffer;
	AVPacket *packet;
	int y_size;
	int ret, got_picture;
	struct SwsContext *img_convert_ctx;


	char filepath[] = "bigbuckbunny_480x272.h265";
	//SDL---------------------------
	int screen_w = 0, screen_h = 0;
	SDL_Window *screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	SDL_Rect sdlRect;

	FILE *fp_yuv;

	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
		printf("Couldn't open input stream.\n");
		return -1;
	}
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoindex = i;
			break;
		}
	if (videoindex == -1) {
		printf("Didn't find a video stream.\n");
		return -1;
	}

	pCodecCtx = pFormatCtx->streams[videoindex]->codec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		printf("Codec not found.\n");
		return -1;
	}
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		printf("Could not open codec.\n");
		return -1;
	}

	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();
	out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
		AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	//Output Info-----------------------------
	printf("--------------- File Information ----------------\n");
	av_dump_format(pFormatCtx, 0, filepath, 0);
	printf("-------------------------------------------------\n");
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

#if OUTPUT_YUV420P 
	fp_yuv = fopen("output.yuv", "wb+");
#endif  

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	//SDL 2.0 Support for multiple windows
	screen = SDL_CreateWindow("Simplest ffmpeg player's Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h,
		SDL_WINDOW_OPENGL);

	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}

	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;

	//SDL End----------------------
	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == videoindex) {
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
			if (ret < 0) {
				printf("Decode Error.\n");
				return -1;
			}
			if (got_picture) {
				sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
					pFrameYUV->data, pFrameYUV->linesize);

#if OUTPUT_YUV420P
				y_size = pCodecCtx->width*pCodecCtx->height;
				fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y 
				fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
				fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
#endif
				//SDL---------------------------
#if 0
				SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
#else
				SDL_UpdateYUVTexture(sdlTexture, &sdlRect,
					pFrameYUV->data[0], pFrameYUV->linesize[0],
					pFrameYUV->data[1], pFrameYUV->linesize[1],
					pFrameYUV->data[2], pFrameYUV->linesize[2]);
#endif	

				SDL_RenderClear(sdlRenderer);
				SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
				SDL_RenderPresent(sdlRenderer);
				//SDL End-----------------------
				//Delay 40ms
				SDL_Delay(40);
			}
		}
		av_free_packet(packet);
	}
	//flush decoder
	//FIX: Flush Frames remained in Codec
	while (1) {
		ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
		if (ret < 0)
			break;
		if (!got_picture)
			break;
		sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
			pFrameYUV->data, pFrameYUV->linesize);
#if OUTPUT_YUV420P
		int y_size = pCodecCtx->width*pCodecCtx->height;
		fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y 
		fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
		fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
#endif
		//SDL---------------------------
		SDL_UpdateTexture(sdlTexture, &sdlRect, pFrameYUV->data[0], pFrameYUV->linesize[0]);
		SDL_RenderClear(sdlRenderer);
		SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
		SDL_RenderPresent(sdlRenderer);
		//SDL End-----------------------
		//Delay 40ms
		SDL_Delay(40);
	}

	sws_freeContext(img_convert_ctx);

#if OUTPUT_YUV420P 
	fclose(fp_yuv);
#endif 

	SDL_Quit();

	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	return 0;
}
int64_t lastReadPacktTime;
static int  interrupt_cb(void *ctx)
{
	int timeout = 3;
	if (av_gettime() - lastReadPacktTime > timeout * 1000 * 1000) {
		return -1;
	}
	return 0;
}

//创建输入上下文
int ffmpeg_in_action::openInput(std::string input) {

	inputContext = avformat_alloc_context();

// 	AVDictionary* options = nullptr;
// 	av_dict_set(&options, "rtsp_transport", "rtmp", 0);
// 
// 	inputContext->interrupt_callback.callback = interrupt_cb;
	int ret = avformat_open_input(&inputContext,input.c_str(),nullptr,nullptr);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Input file open input failed\n");
		return  ret;
	}
	ret = avformat_find_stream_info(inputContext, nullptr);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Find input file stream inform failed\n");
	}
	else
	{
		av_log(NULL, AV_LOG_FATAL, "Open input file  %s success\n", input.c_str());
	}
	return ret;
}



int ffmpeg_in_action::openOutput(std::string output)
{
	int ret = avformat_alloc_output_context2(&outputContext, nullptr, "flv", output.c_str());
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "open output context failed\n");
		return closeOutput();
	}
	ret = avio_open2(&outputContext->pb, output.c_str(), AVIO_FLAG_READ_WRITE, nullptr, nullptr);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "open avio failed");
		return closeOutput();
	}

	for (int i = 0; i < inputContext->nb_streams; i++)
	{
		AVStream* stream = avformat_new_stream(outputContext,inputContext->streams[i]->codec->codec);
		ret = avcodec_copy_context(stream->codec, inputContext->streams[i]->codec);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "copy codec context failed");
			return closeOutput();
		}
	}

	ret = avformat_write_header(outputContext, nullptr);
	if (ret < 0) {
		av_log(NULL,AV_LOG_ERROR,"format  write header failed");
		return closeOutput();
	}
	av_log(NULL,AV_LOG_FATAL,"open output file success %s \n",output.c_str());
	return ret;
}



int ffmpeg_in_action::closeInput()
{
	if (inputContext) {
		avformat_close_input(&inputContext);
	}
	return 1;
}

int  ffmpeg_in_action::closeOutput()
{
	if (outputContext)
	{
		for (int i = 0; i < outputContext->nb_streams; i++)
		{
			avcodec_close(outputContext->streams[i]->codec);
		}
		avformat_close_input(&outputContext);
	}
	return 1;
}

std::shared_ptr<AVPacket> ffmpeg_in_action::readPacketFromSource()
{
	std::shared_ptr<AVPacket> packet(static_cast<AVPacket*>(av_malloc(sizeof(AVPacket))), [&](AVPacket *p) 
	{
		av_packet_free(&p); 
		av_freep(&p);
	});
	av_init_packet(packet.get());
	lastReadPacktTime = av_gettime();
	int ret = av_read_frame(inputContext, packet.get());
	if (ret >= 0) {
		return packet;
	}
	return nullptr;
}

void ffmpeg_in_action::av_packet_rescale_ts(AVPacket *pkt,
	AVRational src_tb, AVRational dst_tb)
{
	if (pkt->pts != AV_NOPTS_VALUE)
		pkt->pts = av_rescale_q(pkt->pts, src_tb, dst_tb);
	if (pkt->dts != AV_NOPTS_VALUE)
		pkt->dts = av_rescale_q(pkt->dts, src_tb, dst_tb);
	if (pkt->duration > 0)
		pkt->duration = av_rescale_q(pkt->duration, src_tb, dst_tb);
}

int ffmpeg_in_action::writePacket(std::shared_ptr<AVPacket> packet)
{
	auto inputStream = inputContext->streams[packet->stream_index];
	auto outputStream = outputContext->streams[packet->stream_index];
	av_packet_rescale_ts(packet.get(), inputStream->time_base, outputStream->time_base);
	return av_interleaved_write_frame(outputContext, packet.get());
}

void ffmpeg_in_action::cutFile()
{
	//第20S开始，去掉8S
	int startPacketNum = 500;
	int  discardtPacketNum = 200;
	int packetCount = 0;
	int64_t lastPacketPts = AV_NOPTS_VALUE;
	int64_t lastPts = AV_NOPTS_VALUE;

	int ret = openInput("E:\\develop\\ffmpeg-in-action\\bin\\test.flv");
	if (ret >= 0)
	{
		ret = openOutput("E:\\develop\\ffmpeg-in-action\\bin\\out1.flv");
	}
	else
	{
		closeInput();
		closeOutput();
		std::cout << "open file error" << std::endl;
	}
	
	while (true)
	{
		auto packet = readPacketFromSource();
		if (packet)
		{
			packetCount++;
			if (packetCount <= 500 || packetCount >= 700)
			{
				if (packetCount >= 700)
				{
					if (packet->pts - lastPacketPts > 120)
					{
						lastPts = lastPacketPts;
					}
					else
					{
						auto diff = packet->pts - lastPacketPts;
						lastPts += diff;
					}
				}
				lastPacketPts = packet->pts;
				if (lastPts != AV_NOPTS_VALUE)
				{
					packet->pts = packet->dts = lastPts;
				}
				ret = writePacket(packet);
			}
		}
		else
		{
			break;
		}
	}
	std::cout << "Cut File End " << std::endl;
}

AVFormatContext * ffmpeg_in_action::getInputContext()
{
	return inputContext;
}

AVFormatContext * ffmpeg_in_action::getOutputContext()
{
	return outputContext;
}

void ffmpeg_in_action::doSave()
{
	init();
	int ret = openInput("rtsp://169.254.51.14:8554/channel=0");
	if (ret >= 0)
	{
		//转发 rtmp://127.0.0.1:1935/live/stream0  输出类型应该为 flv
		//保存到本地 E:\\xiazai0321.ts 输出类型设置为 mpegts
		ret = openOutput("rtmp://192.168.1.110/myapp/mystream");
		
	}
	else {
		closeInput();
		closeOutput();
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::seconds(100));
		}
	}
	
	while (true)
	{
		auto packet = readPacketFromSource();
		if (packet)
		{
			ret = writePacket(packet);
			if (ret >= 0)
			{
				std::cout << "WritePacket Success!" << std::endl;
			}
			else
			{
				std::cout << "WritePacket failed!" << std::endl;
			}
		}
		else
		{
			break;
		}
	}
}

void ffmpeg_in_action::on_sdl_play_btn_clicked()
{
	SDL2Player();
}

void ffmpeg_in_action::on_save_stream_btn_clicked()
{
	doSave();
}

void ffmpeg_in_action::on_cutfile_btn_clicked()
{
	cutFile();
}
