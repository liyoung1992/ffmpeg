#include "ffmpeg_in_action.h"
#include <QDebug>
#ifdef __cplusplus
// C++中使用av_err2str宏
char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
#define av_err2str(errnum)     av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)
#endif


#define ADTS_HEADER_LEN 7
void adts_header(char *szAdtsHeader, int dataLen)
{

	int audio_object_type = 2;
	int sampling_frequency_index = 7;
	int channel_config = 2;

	int adtsLen = dataLen + 7;

	szAdtsHeader[0] = 0xff;      //syncword:0xfff                          高8bits
	szAdtsHeader[1] = 0xf0;      //syncword:0xfff                          低4bits
	szAdtsHeader[1] |= (0 << 3); //MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
	szAdtsHeader[1] |= (0 << 1); //Layer:0                                 2bits
	szAdtsHeader[1] |= 1;        //protection absent:1                     1bit

	szAdtsHeader[2] = (audio_object_type - 1) << 6;            //profile:audio_object_type - 1                      2bits
	szAdtsHeader[2] |= (sampling_frequency_index & 0x0f) << 2; //sampling frequency index:sampling_frequency_index  4bits
	szAdtsHeader[2] |= (0 << 1);                               //private bit:0                                      1bit
	szAdtsHeader[2] |= (channel_config & 0x04) >> 2;           //channel configuration:channel_config               高1bit

	szAdtsHeader[3] = (channel_config & 0x03) << 6; //channel configuration:channel_config      低2bits
	szAdtsHeader[3] |= (0 << 5);                    //original：0                               1bit
	szAdtsHeader[3] |= (0 << 4);                    //home：0                                   1bit
	szAdtsHeader[3] |= (0 << 3);                    //copyright id bit：0                       1bit
	szAdtsHeader[3] |= (0 << 2);                    //copyright id start：0                     1bit
	szAdtsHeader[3] |= ((adtsLen & 0x1800) >> 11);  //frame length：value   高2bits

	szAdtsHeader[4] = (uint8_t)((adtsLen & 0x7f8) >> 3); //frame length:value    中间8bits
	szAdtsHeader[5] = (uint8_t)((adtsLen & 0x7) << 5);   //frame length:value    低3bits
	szAdtsHeader[5] |= 0x1f;                             //buffer fullness:0x7ff 高5bits
	szAdtsHeader[6] = 0xfc;
}
#define ERROR_STR_SIZE 1024

ffmpeg_in_action::ffmpeg_in_action(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	init();

	int err_code;
	char errors[1024];

	char *src_filename = NULL;
	char *dst_filename = NULL;


	int audio_stream_index = -1;
	int len;

	AVFormatContext *ofmt_ctx = NULL;
	AVOutputFormat *output_fmt = NULL;

	AVStream *in_stream = NULL;
	AVStream *out_stream = NULL;

	AVFormatContext *fmt_ctx = NULL;
	//AVFrame *frame = NULL;
	AVPacket pkt;

	av_log_set_level(AV_LOG_DEBUG);



	src_filename = "D:\\test.mp4";
	dst_filename = "D:\\test1.aac";



	/*register all formats and codec*/
	av_register_all();

	/*open input media file, and allocate format context*/
	if ((err_code = avformat_open_input(&fmt_ctx, src_filename, NULL, NULL)) < 0) {
		av_strerror(err_code, errors, 1024);
		av_log(NULL, AV_LOG_DEBUG, "Could not open source file: %s, %d(%s)\n",
			src_filename,
			err_code,
			errors);
		return ;
	}

	/*retrieve audio stream*/
	if ((err_code = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
		av_strerror(err_code, errors, 1024);
		av_log(NULL, AV_LOG_DEBUG, "failed to find stream information: %s, %d(%s)\n",
			src_filename,
			err_code,
			errors);
		return ;
	}

	/*dump input information*/
	av_dump_format(fmt_ctx, 0, src_filename, 0);

	in_stream = fmt_ctx->streams[1];
	AVCodecParameters *in_codecpar = in_stream->codecpar;
	if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
		av_log(NULL, AV_LOG_ERROR, "The Codec type is invalid!\n");
		exit(1);
	}

	//out file
	ofmt_ctx = avformat_alloc_context();
	output_fmt = av_guess_format(NULL, dst_filename, NULL);
	if (!output_fmt) {
		av_log(NULL, AV_LOG_DEBUG, "Cloud not guess file format \n");
		exit(1);
	}

	ofmt_ctx->oformat = output_fmt;

	out_stream = avformat_new_stream(ofmt_ctx, NULL);
	if (!out_stream) {
		av_log(NULL, AV_LOG_DEBUG, "Failed to create out stream!\n");
		exit(1);
	}

	if (fmt_ctx->nb_streams < 2) {
		av_log(NULL, AV_LOG_ERROR, "the number of stream is too less!\n");
		exit(1);
	}


	if ((err_code = avcodec_parameters_copy(out_stream->codecpar, in_codecpar)) < 0) {
		av_strerror(err_code, errors, ERROR_STR_SIZE);
		av_log(NULL, AV_LOG_ERROR,
			"Failed to copy codec parameter, %d(%s)\n",
			err_code, errors);
	}

	out_stream->codecpar->codec_tag = 0;

	if ((err_code = avio_open(&ofmt_ctx->pb, dst_filename, AVIO_FLAG_WRITE)) < 0) {
		av_strerror(err_code, errors, 1024);
		av_log(NULL, AV_LOG_DEBUG, "Could not open file %s, %d(%s)\n",
			dst_filename,
			err_code,
			errors);
		exit(1);
	}



	/*dump output information*/
	av_dump_format(ofmt_ctx, 0, dst_filename, 1);



	/*initialize packet*/
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	/*find best audio stream*/
	audio_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (audio_stream_index < 0) {
		av_log(NULL, AV_LOG_DEBUG, "Could not find %s stream in input file %s\n",
			av_get_media_type_string(AVMEDIA_TYPE_AUDIO),
			src_filename);
		return ;
	}

	if (avformat_write_header(ofmt_ctx, NULL) < 0) {
		av_log(NULL, AV_LOG_DEBUG, "Error occurred when opening output file");
		exit(1);
	}

	/*read frames from media file*/
	while (av_read_frame(fmt_ctx, &pkt) >= 0) {
		if (pkt.stream_index == audio_stream_index) {
			pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt.dts = pkt.pts;
			pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
			pkt.pos = -1;
			pkt.stream_index = 0;
			av_interleaved_write_frame(ofmt_ctx, &pkt);
			av_packet_unref(&pkt);
		}
	}

	av_write_trailer(ofmt_ctx);

	/*close input media file*/
	avformat_close_input(&fmt_ctx);
// 	if (dst_fd) {
// 		fclose(dst_fd);
// 	}

	avio_close(ofmt_ctx->pb);

	return ;

// 	抽取音频
// 		AVFormatContext* fmt_ctx = NULL;
// 		AVPacket pkt;
// 		int audio_index;
// 		int ret = avformat_open_input(&fmt_ctx, "D:\\1111.mp4", NULL, NULL);
// 		if (ret < 0) {
// 			av_log(NULL, AV_LOG_ERROR, "failed to open file,%s\n",av_err2str(ret));
// 		}
// 		av_dump_format(fmt_ctx, 0, "D:\\test.mp4", 0);
// 	
// 		//GET stream
// 		FILE* dst_df = fopen("D:\\11112.aac", "wb");
// 		if (!dst_df) {
// 			av_log(NULL,AV_LOG_ERROR,"can't open");
// 			avformat_close_input(&fmt_ctx);
// 			return;
// 		}
// 		ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1. - 1, NULL, 0,NULL);
// 		if (ret < 0) {
// 			av_log(NULL,AV_LOG_ERROR,"best failed");
// 			avformat_close_input(&fmt_ctx);
// 			fclose(dst_df);
// 			return;
// 		}
// 		audio_index = ret;
// 		av_init_packet(&pkt);
// 		while (av_read_frame(fmt_ctx, &pkt) >= 0)
// 		{
// 			if (pkt.stream_index == audio_index) {
// 				char adts_header_buf[7];
// 				adts_header(adts_header_buf,pkt.size);
// 				fwrite(adts_header_buf, sizeof(char), 7, dst_df);
// 				int len =  fwrite(pkt.data,1,pkt.size,dst_df);
// 				if (len != pkt.size) {
// 					av_log(NULL,AV_LOG_ERROR,"len != pkt size\n");
// 				}
// 			}
// 			av_packet_unref(&pkt);
// 		}
// 		//av_read_frame(fmt_ctx,&pkt);
// 		avformat_close_input(&fmt_ctx);
// 		if (dst_df) {
// 			fclose(dst_df);
// 		}

// 	//打印音视频信息
// 	AVFormatContext* fmt_ctx = NULL;
// 	int ret = avformat_open_input(&fmt_ctx, "D:\\test.mp4", NULL, NULL);
// 	if (ret < 0) {
// 		av_log(NULL, AV_LOG_ERROR, "failed to open file,%s\n",av_err2str(ret));
// 	}
// 	av_dump_format(fmt_ctx, 0, "D:\\test.mp4", 0);
// 	avformat_close_input(&fmt_ctx);
	// file opt
// 	int ret = avpriv_io_delete("D://test.ts");
// 	if (ret < 0) {
// 		av_log(NULL, AV_LOG_ERROR, "failed to delete file test.ts");
// 		return;
// 	}
// 	int ret = avpriv_io_move("D://aac.ts", "D://aac_move.ts");
// 	if (ret < 0) {
// 		av_log(NULL, AV_LOG_ERROR, "failed to move file aac.ts");
// 		return;
// 	}
// 	return;

	 // dir
// 	AVIODirContext* ctx = NULL;
// 	AVIODirEntry* entry = NULL;
// 	//注意Windows下会返回-40，也就是Function not implement,
// 	//windows不支持此函数
// 	int ret = avio_open_dir(&ctx, "D:/wamp", NULL);
// 	if (ret < 0) {
// 		av_log(NULL, AV_LOG_ERROR, "failed to open dir: %d\n",(ret));
// 		return;
// 	}
// 	while (true)
// 	{
// 		ret = avio_read_dir(ctx, &entry);
// 		if (ret < 0) {
// 			av_log(NULL, AV_LOG_ERROR, "failed read dir:%d\n", (ret));
// 			avio_close_dir(&ctx);
// 			return;
// 		}
// 		if (!entry) {
// 			break;
// 		}
// 		av_log(NULL, AV_LOG_INFO, "%12" PRId64 "%s \n", entry->size,entry->name);
// 		//释放
// 		avio_free_directory_entry(&entry);
// 	}
// 	avio_close_dir(&ctx);
// 	return;
}

/*
	初始化ffmpeg相关配置
*/
void ffmpeg_in_action::init()
{
	av_register_all();
	avfilter_register_all();
	avformat_network_init();
//	av_log_set_level(AV_LOG_ERROR);
	av_log_set_level(AV_LOG_DEBUG);
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

void ffmpeg_in_action::getAudioData()
{

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
