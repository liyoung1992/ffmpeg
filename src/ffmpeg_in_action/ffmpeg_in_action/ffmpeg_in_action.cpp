#include "ffmpeg_in_action.h"
#include <time.h>
#include <QDebug>
#ifdef __cplusplus
// C++中使用av_err2str宏
char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
#define av_err2str(errnum)     av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)
char ts_str[AV_TS_MAX_STRING_SIZE] = { 0 };
#define  av_ts2str(ts)   av_ts_make_string(ts_str, ts)
/*char ts_str[AV_TS_MAX_STRING_SIZE] = { 0 };*/
#define  av_ts2timestr(ts,tb) av_ts_make_time_string(ts_str, ts, tb)
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


#ifndef AV_WB32
#   define AV_WB32(p, val) do {                 \
uint32_t d = (val);                     \
((uint8_t*)(p))[3] = (d);               \
((uint8_t*)(p))[2] = (d)>>8;            \
((uint8_t*)(p))[1] = (d)>>16;           \
((uint8_t*)(p))[0] = (d)>>24;           \
} while(0)
#endif

#ifndef AV_RB16
#   define AV_RB16(x)                           \
((((const uint8_t*)(x))[0] << 8) |          \
((const uint8_t*)(x))[1])
#endif

static int alloc_and_copy(AVPacket *out,
	const uint8_t *sps_pps,
	uint32_t sps_pps_size,
	const uint8_t *in,
	uint32_t in_size) {
	uint32_t offset = out->size;
	//SPS和PPS的startCode是00 00 00 01  非SPS和PPS的startCode是00 00 01
	uint8_t nal_header_size = offset ? 3 : 4;
	int err;

	//av_grow_packet增大数组缓存空间  就是扩容的意思
	err = av_grow_packet(out, sps_pps_size + in_size + nal_header_size);
	if (err < 0) {
		return err;
	}

	if (sps_pps) {
		memcpy(out->data + offset, sps_pps, sps_pps_size);
	}

	memcpy(out->data + sps_pps_size + nal_header_size + offset, in, in_size);
	if (!offset) {
		AV_WB32(out->data + sps_pps_size, 1);
	}
	else {
		(out->data + offset + sps_pps_size)[0] =
			(out->data + offset + sps_pps_size)[1] = 0;
		(out->data + offset + sps_pps_size)[2] = 1;
	}
	return 0;
}

/** SPS PPS
 * 正常SPS和PPS包含在FLV的AVCDecoderConfigurationRecord结构中，而AVCDecoderConfigurationRecord就是经过FFmpeg分析后，
 就是AVCodecContext里面的extradata

 *
 * 处理SPS-PPS
 * 第一种方法:
 * h264_extradata_to_annexb
	从AVPacket中的extra_data 获取并组装
	添加startCode
 * 第二种方法:  在新的FFmpeg中不建议使用会造成内存漏的问题
	AVBitStreamFilterContext *h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");  //定义放在循环外面
	av_bitstream_filter_filter(h264bsfc, fmt_ctx->streams[in->stream_index]->codec, NULL, &spspps_pkt.data, &spspps_pkt.size, in->data, in->size, 0);  //解析sps pps

 * 第三种方法 新版FFmpeg中使用
	 AVBitStreamFilter和AVBSFContext
	  如方法 bl_decode
 *
 *
 * 为什么需要处理sps和pps startCode,其实是因为H.264有两种封装格式
 * 1.Annexb模式 传统模式 有startCode SPS和PPS是在ES中   这种一般都是网络比特流
 * 2.MP4模式  一般mp4 mkv会有，没有startcode SPS和PPS以及其它信息被封装在container中，
 每一个frame前面是这个frame的长度。很多解码器只支持Annexb这种模式，因为需要将MP4做转换  用于保存文件
 */
int h264_extradata_to_annexb(const uint8_t *codec_extradata, const int codec_extradata_size, AVPacket *out_extradata, int padding) {
	uint16_t unit_size;
	uint64_t total_size = 0;
	uint8_t *out = NULL, unit_nb, sps_done = 0, sps_seen = 0, pps_seen = 0, sps_offset = 0, pps_offset = 0;

	const uint8_t *extradata = codec_extradata + 4;
	static const uint8_t nalu_header[4] = { 0, 0, 0, 1 };
	int length_size = (*extradata++ & 0x3) + 1; //retrieve length coded size 用于指示表示编码数据长度所需字节数

	sps_offset = pps_offset = -1;

	/**retrieve sps and pps unit(s)*/
	unit_nb = *extradata++ & 0x1f; /** number of sps unit(s) 查看有多少个sps和pps 一般情况下只有一个*/
	if (!unit_nb) {
		goto pps;
	}
	else {
		sps_offset = 0;
		sps_seen = 1;
	}

	while (unit_nb--) {
		int err;

		unit_size = AV_RB16(extradata);
		total_size += unit_size + 4;
		if (total_size > INT_MAX - padding) {
			av_log(NULL, AV_LOG_DEBUG, "too big extradata size ,corrupted stream or invalid MP4/AVCC bitstream\n");
			av_free(out);
			return AVERROR(EINVAL);
		}

		if (extradata + 2 + unit_size > codec_extradata + codec_extradata_size) {
			av_log(NULL, AV_LOG_DEBUG, "Packet header is not contained in global extradata,corrupt stream or invalid MP4/AVCC bitstream\n");
			av_free(out);
			return AVERROR(EINVAL);
		}

		if ((err = av_reallocp(&out, total_size + padding)) < 0) {
			return err;
		}

		memcpy(out + total_size - unit_size - 4, nalu_header, 4);
		memcpy(out + total_size - unit_size, extradata + 2, unit_size);
		extradata += 2 + unit_size;
	pps:
		if (!unit_nb && !sps_done++) {
			unit_nb = *extradata++;
			if (unit_nb) {
				pps_offset = total_size;
				pps_seen = 1;
			}
		}
	}

	if (out) {
		memset(out + total_size, 0, padding);
	}

	if (!sps_seen) {
		av_log(NULL, AV_LOG_WARNING, "Warning SPS NALU missing or invalid.The resulting stream may not paly\n");
	}

	if (!pps_seen) {
		av_log(NULL, AV_LOG_WARNING, "Warning pps nalu missing or invalid\n");
	}

	out_extradata->data = out;
	out_extradata->size = total_size;
	return length_size;
}

int h264_mp4toannexb(AVFormatContext *fmt_ctx, AVPacket *in, FILE *dst_fd) {
	AVPacket *out = NULL;
	AVPacket spspps_pkt;

	int len;
	uint8_t unit_type;
	int32_t nal_size;
	uint32_t cumul_size = 0;
	const uint8_t *buf;
	const uint8_t *buf_end;
	int buf_size;
	int ret = 0, i;

	out = av_packet_alloc();

	buf = in->data;
	buf_size = in->size;
	buf_end = in->data + in->size;

	AVBitStreamFilterContext *h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");

	do {
		ret = AVERROR(EINVAL);
		if (buf + 4 > buf_end) { //越界
			goto fail;
		}

		//AVPacket的前4个字节 算出的是nalu的长度  因为AVPacket内可能有一帧也可能有多帧
		for (nal_size = 0, i = 0; i < 4; i++) {
			nal_size = (nal_size << 8) | buf[i]; //<<8 低地址32位的高位  高地址实际上是32位的低位
		}

		buf += 4;
		//第一个字节的后五位是type
		unit_type = *buf & 0x1f;  //NAL Header中的第5个字节表示type   0x1f取出后5位

		if (nal_size > buf_end - buf || nal_size < 0) {
			goto fail;
		}

		/**prepend only to the first type 5 NAL unit of an IDR picture, if no sps/pps are already present*/
		/**IDR
		 * 一个序列的第一个图像叫做IDR图像(立即刷新图像) IDR图像都是I帧图像
		 * I和IDR帧都使用帧内预测，I帧不用参考任何帧，但是之后的P帧和B帧是有可能参考这个I帧之前的帧的。IDR不允许
		 *
		 * IDR的核心作用:
		 * H.264引入IDR图像是为了解码的重同步，当解码器解码到IDR图像时，立即将将参考帧队列清空，将已解码的数据全部输出或抛弃。重新查找参数集，开始一个新的序列。这样，如果前一个序列出现重大错误，在这里可以获得重新同步的机会。IDR图像之后的图像永远不会使用IDR之前的图像的数据来解码
		 */
		if (unit_type == 5) {


			//sps pps
			/*h264_extradata_to_annexb(fmt_ctx->streams[in->stream_index]->codec->extradata,
									 fmt_ctx->streams[in->stream_index]->codec->extradata_size,
									 &spspps_pkt,
									 AV_INPUT_BUFFER_PADDING_SIZE);*/
			av_bitstream_filter_filter(h264bsfc, fmt_ctx->streams[in->stream_index]->codec, NULL, &spspps_pkt.data, &spspps_pkt.size, in->data, in->size, 0);
			//startcode
			if ((ret = alloc_and_copy(out,
				spspps_pkt.data,
				spspps_pkt.size,
				buf,
				nal_size)) < 0) {
				goto fail;
			}
		}
		else { //非关建帧
			if ((ret = alloc_and_copy(out, NULL, 0, buf, nal_size)) < 0) {
				goto fail;
			}
		}

		len = fwrite(out->data, 1, out->size, dst_fd);
		if (len != out->size) {
			av_log(NULL, AV_LOG_DEBUG, "warning,length of writed data isn't equal pkt.size(%d,%d)\n", len,
				out->size);
		}
		fflush(dst_fd);

	next_nal:
		buf += nal_size;
		cumul_size += nal_size + 4;
	} while (cumul_size < buf_size);

fail:
	av_packet_free(&out);
	return ret;
}


int bl_decode(AVFormatContext *fmt_ctx, AVPacket *in, FILE *dst_fd) {
	int len = 0;

	const AVBitStreamFilter *absFilter = av_bsf_get_by_name("h264_mp4toannexb");
	AVBSFContext *absCtx = NULL;
	AVCodecParameters *codecpar = NULL;

	av_bsf_alloc(absFilter, &absCtx);

	if (fmt_ctx->streams[in->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
		codecpar = fmt_ctx->streams[in->stream_index]->codecpar;
	}
	else {
		return -1;
	}

	avcodec_parameters_copy(absCtx->par_in, codecpar);

	av_bsf_init(absCtx);

	if (av_bsf_send_packet(absCtx, in) != 0) {
		av_bsf_free(&absCtx);
		absCtx = NULL;
		return -1;
	}

	while (av_bsf_receive_packet(absCtx, in) == 0) {
		len = fwrite(in->data, 1, in->size, dst_fd);
		if (len != in->size) {
			av_log(NULL, AV_LOG_DEBUG, "warning,length of writed data isn't equal pkt.size(%d,%d)\n", len,
				in->size);
		}
		fflush(dst_fd);
	}
	av_bsf_free(&absCtx);
	absCtx = NULL;
	return 0;
}


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
		return;
	}

	/*retrieve audio stream*/
	if ((err_code = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
		av_strerror(err_code, errors, 1024);
		av_log(NULL, AV_LOG_DEBUG, "failed to find stream information: %s, %d(%s)\n",
			src_filename,
			err_code,
			errors);
		return;
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
		return;
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

	return;
}

void ffmpeg_in_action::getVideoData()
{
	int err_code;
	char errors[1024];

	char *src_filename = NULL;
	char *dst_filename = NULL;

	FILE *dst_fd = NULL;
	int video_stream_index = -1;

	AVFormatContext *fmt_ctx = NULL;
	AVPacket pkt;

	av_log_set_level(AV_LOG_DEBUG);



	src_filename = "D:\\123.mp4";
	dst_filename = "D:\\123.h264";

	if (src_filename == NULL || dst_filename == NULL) {
		av_log(NULL, AV_LOG_DEBUG, "src or dts file is null\n");
		return;
	}

	av_register_all();
	dst_fd = fopen(dst_filename, "wb");
	if (!dst_fd) {
		av_log(NULL, AV_LOG_DEBUG, "could not open destination file:%s\n", dst_filename);
		return;
	}

	/**open input media file, and allocate format context*/
	if ((err_code = avformat_open_input(&fmt_ctx, src_filename, NULL, NULL)) < 0) {
		av_strerror(err_code, errors, 1024);
		av_log(NULL, AV_LOG_DEBUG, "Could not open source file:%s, %d(%s)\n",
			src_filename,
			err_code,
			errors);
		return;
	}

	/**dump input information*/
	av_dump_format(fmt_ctx, 0, src_filename, 0);

	/**initialize packet*/
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	/**find best video streams*/
	video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (video_stream_index < 0) {
		av_log(NULL, AV_LOG_DEBUG, "could not find %s stream in input file %s\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO),
			src_filename);
		return;
	}

	/**read frames from media file*/
	while (av_read_frame(fmt_ctx, &pkt) >= 0) {
		if (pkt.stream_index == video_stream_index) {
			//h264_mp4toannexb(fmt_ctx, &pkt, dst_fd);
			bl_decode(fmt_ctx, &pkt, dst_fd);
		}
		av_packet_unref(&pkt);
	}

	/**close input media file*/
	avformat_close_input(&fmt_ctx);
	if (dst_fd) {
		fclose(dst_fd);
	}
}

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag) {
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

	printf("%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
		tag,
		av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
		av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
		av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
		pkt->stream_index);
}
int ffmpeg_in_action::mp4ToFlv()
{
	//mp4-->flv
	//输出上下文
	//avformat_alloc_output_context2()

	//释放输出上下文
	//avform_free_context()

	//新建新的流
	//avformat_new_stream
	//复制参数
	//accodec_parameters_copy

	//写多媒体文件头
	//avformat_write_header
	//写文件数据
	//av_write_frame
	//av_interleaved_wirte_frame（交叉写入）
	//写尾部
	//av_write_tailer
	AVOutputFormat *ofmt = NULL;
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;
	const char *in_filename, *out_filename;
	int ret, i;
	int stream_index = 0;
	int *stream_mapping = NULL;
	int stream_mapping_size = 0;


	in_filename =  "D:\\123.mp4";
	out_filename =	"D:\\123.flv";

	av_register_all();
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) { //创建输入文件上下文
		fprintf(stderr, "Could not open input file %s", in_filename);
		goto end;
	}

	/**avformat_find_stream_info() 该函数可以读取一部分音视频数据并且获得一些相关的信息 探测文件信息*/
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		fprintf(stderr, "Failed to retrieve input stream information");
		goto end;
	}
	av_dump_format(ifmt_ctx, 0, in_filename, 0);

	//创建输出文件上下文
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	if (!ofmt_ctx) {
		fprintf(stderr, "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}

	stream_mapping_size = ifmt_ctx->nb_streams;
	stream_mapping = (int *)av_mallocz_array(stream_mapping_size, sizeof(*stream_mapping));
	//申请空间
	if (!stream_mapping) {
		ret = AVERROR(ENOMEM);
		goto end;
	}

	//AVOutputFormat 输出端的信息 是FFmpeg解复用(解封装)用的结构体，比如，输出的的协议，输出的编解码器
	ofmt = ofmt_ctx->oformat;

	//就是把AVStream 读取到内存中
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		AVStream *out_stream;
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVCodecParameters *in_codecpar = in_stream->codecpar; //AVCodecParameters 用于记录编码后的流信息，即通道中存储的流的编码信息

		if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
			in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
			in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
			stream_mapping[i] = -1;
			continue;
		}
		stream_mapping[i] = stream_index++;

		//在AVFormatContext中创建Stream通道，用于记录通道信息
		out_stream = avformat_new_stream(ofmt_ctx, NULL);
		if (!out_stream) {
			fprintf(stderr, "Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
		if (ret < 0) {
			fprintf(stderr, "Failed to copy codec parameters\n");
			goto end;
		}

		out_stream->codecpar->codec_tag = 0;
	}

	av_dump_format(ofmt_ctx, 0, out_filename, 1);

	if (!(ofmt->flags & AVFMT_NOFILE)) {
		/** avio_open() / avio_open2()
		 * 用于打开FFmpeg的输入输出文件
		 * 参数1:函数调用成功之后创建的AVIOContext结构体
		 * 参数2:输入输出协议的地址(文件路径)
		 * 参数3:打开地址的方式   AVIO_FLAG_READ 只读  AVIO_FLAG_WRITE 只写  AVIO_FLAG_READ_WRITE 读写
		 *
		 * 功能:
		 * avio_open2() 内部主要调用两个函数:ffurl_open() 和ffio_fdopen(), 其中ffurl_open()用于初始化URLContext,ffio_fdopen()用于根据URLContext初始化AVIOContext. URLContext中包含的URLProtocol完成了具体的协议读写等工作。AVIOContext则是在URLContext的读写函数外面加上一层'包装'(通过retry_transfer_wrapper()函数)
		 * URLProtocol 主要包含用于协议读写的函数指针 url_open() url_read() url_write() url_close
		 */
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open output file '%s'", out_filename);
			goto end;
		}
	}

	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
		goto end;
	}

	//去内存中取
	while (1) {
		AVStream *in_stream, *out_stream;
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0) {
			break;
		}

		in_stream = ifmt_ctx->streams[pkt.stream_index];
		if (pkt.stream_index >= stream_mapping_size || stream_mapping[pkt.stream_index] < 0) {
			av_packet_unref(&pkt);
			continue;
		}

		pkt.stream_index = stream_mapping[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];
		log_packet(ifmt_ctx, &pkt, "in"); //打印

		/**不同时间基计算
		 * av_rescale_q(a, b, c)
		 * av_rescale_q_rnd(a, b, c, AVRoundion rnd) //AVRoundion 就是取整的方式
		 * 作用:
			把时间戳从一个时基调整到另外一个时基时候用的函数。其中，a表示要换算的值，b表式原来的时间基，c表示要转换的时间基， 其计算公式是 a * b / c
		 *
		 * 时间戳转秒
		 *  time_in_seconds = av_q2q(AV_TIME_BASE_Q) * timestamp
		 *
		 * 秒转时间戳
		 *  timestamp = AV_TIME_BASE * time_in_seconds
		 */
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);

		pkt.pos = -1;

		log_packet(ofmt_ctx, &pkt, "out");

		ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
		if (ret < 0) {
			fprintf(stderr, "Error muxing packet\n");
			break;
		}
		av_packet_unref(&pkt);
	}

	av_write_trailer(ofmt_ctx);
end:
	avformat_close_input(&ifmt_ctx); //关闭输入文件上下文

	if (ofmt_ctx && !(ofmt->flags && AVFMT_NOFILE)) {
		avio_closep(&ofmt_ctx->pb);
	}

	//释放输出文件的上下文
	avformat_free_context(ofmt_ctx);

	av_freep(&stream_mapping);

	if (ret < 0 && ret != AVERROR_EOF) {
		fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
		return -1;
	}

	return 0;


}

int ffmpeg_in_action::seekCutFile()
{
	//跳一段时间
	//av_seek_frame()
	//开始，结束点
	double from_seconds = 20;
	double end_seconds= 60;
	char *in_filename;
	char *out_filename;
	in_filename = "D:\\test.mp4";
	out_filename = "D:\\cut.mp4";

	AVOutputFormat *ofmt = NULL; //输出流的各种格式的合起的
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;

	AVPacket pkt;
	int ret, i;

	av_register_all();

	/**step1 打开输入文件  AVFormatContext*/
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		fprintf(stderr, "Could not open input file %s", in_filename);
		goto end;
	}


	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) { //试探的作用
		fprintf(stderr, "Failed to retrieve input stream information");
		goto end;
	}

	av_dump_format(ifmt_ctx, 0, in_filename, 0); //打印

	/** step2  打开输出文件 AVFormatContext **/
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	if (!ofmt_ctx) {
		fprintf(stderr, "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}

	/**step3 创建新的AVStrem 参数的拷贝 然后放到ofmt_ctx中  并设置flags*/
	ofmt = ofmt_ctx->oformat;
	/*
	for (i = 0; i < ifmt_ctx->nb_streams; i ++) {
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (!out_stream) {
			fprintf(stderr, "Failed allocation output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0) {
			fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
			goto end;
		}

		//对于这个flag的处理，非常重要
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {   // 0x0040 AVFMT_GLOBALHEADER
			out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;  //1 << 22  0x200000
		}
	}*/
	for (i = 0; i < ifmt_ctx->nb_streams; i++) { //跟上面是一样的代码 只是使用了新的API
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVCodec *in_codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_codec);

		if (!out_stream) {
			fprintf(stderr, "Failed allocation output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		AVCodecContext *in_codec_context = avcodec_alloc_context3(in_codec);
		ret = avcodec_parameters_to_context(in_codec_context, in_stream->codecpar);
		if (ret < 0) {
			printf("Failed to copy in_stream codecpar to codec context\n");
			avcodec_free_context(&in_codec_context);
			goto end;
		}

		in_codec_context->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
			in_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}

		ret = avcodec_parameters_from_context(out_stream->codecpar, in_codec_context);
		if (ret < 0) {
			printf("Failed to copy codec context to out_stream codecpar context\n");
			avcodec_free_context(&in_codec_context);
			goto end;
		}
		avcodec_free_context(&in_codec_context);
	}

	av_dump_format(ofmt_ctx, 0, out_filename, 1);

	/** step4 打开 输出文件*/
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open output file %s", out_filename);
			goto end;
		}
	}

	/**step 6 wirte 文件*/
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
		goto end;
	}


	/**step5 seek到指定位置 */
	ret = av_seek_frame(ifmt_ctx, -1, from_seconds * AV_TIME_BASE, AVSEEK_FLAG_ANY);
	if (ret < 0) {
		fprintf(stderr, "Error seek\n");
		goto end;
	}

	int64_t *dts_start_from = (int64_t *)malloc(sizeof(int64_t) * ifmt_ctx->nb_streams);
	memset(dts_start_from, 0, sizeof(int64_t) *ifmt_ctx->nb_streams);

	int64_t *pts_start_from = (int64_t *)malloc(sizeof(int64_t) * ifmt_ctx->nb_streams);
	memset(pts_start_from, 0, sizeof(int64_t) * ifmt_ctx->nb_streams);

	while (1) {
		AVStream *in_stream, *out_stream;
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0) {
			break;
		}

		in_stream = ifmt_ctx->streams[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];

		log_packet(ifmt_ctx, &pkt, "in");

		if (av_q2d(in_stream->time_base) * pkt.pts > end_seconds) { //需要裁剪最后的时间
			av_packet_unref(&pkt);
			break;
		}

		if (dts_start_from[pkt.stream_index] == 0) {   //保存dts
			dts_start_from[pkt.stream_index] = pkt.dts;
			printf("dts_start_from: %s\n", av_ts2str(dts_start_from[pkt.stream_index]));
		}

		if (pts_start_from[pkt.stream_index] == 0) { //保存pts
			pts_start_from[pkt.stream_index] = pkt.pts;
			printf("pts_start_from:%s\n", av_ts2str(pts_start_from[pkt.stream_index]));
		}


		/** copy packet*/
		// pkt.pts = av_rescale_q_rnd(pkt.pts - pts_start_from[pkt.stream_index], in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
		// pkt.dts = av_rescale_q_rnd(pkt.dts - dts_start_from[pkt.stream_index], in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);

		//这种写法跟上面的并没有差异
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
			AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, 
			AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		if (pkt.pts < 0) {
			pkt.pts = 0;
		}

		if (pkt.dts < 0) {
			pkt.dts = 0;
		}

		pkt.duration = (int)av_rescale_q((int64_t)pkt.duration, in_stream->time_base, out_stream->time_base);

		pkt.pos = -1;
		log_packet(ofmt_ctx, &pkt, "out");
		printf("\n");


		if (pkt.pts >= pkt.dts) { //这个判断是不处理B帧   正确的方法是先解码再编码，然后再裁剪，可以利用AVFrame的pict_type是否等于AV_PICTURE_TYPE_B
			ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
			if (ret < 0) {
				fprintf(stderr, "Error muxing packet\n");
				av_packet_unref(&pkt);
				break;
			}
		}
		av_packet_unref(&pkt);
	}

	free(dts_start_from);
	free(pts_start_from);

	av_write_trailer(ofmt_ctx);

end:
	avformat_close_input(&ifmt_ctx);

	/** close output*/
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE)) {
		avio_closep(&ofmt_ctx->pb);
	}

	avformat_free_context(ofmt_ctx);

	if (ret < 0 && ret != AVERROR_EOF) {
		fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
		return 1;
	}
	return 0;
}


//decode
#define INBUF_SIZE 4096

#define WORD uint16_t
#define DWORD uint32_t
#define LONG int32_t

#pragma pack(2)
typedef struct tagBITMAPFILEHEADER {
	WORD  bfType;
	DWORD bfSize;
	WORD  bfReserved1;
	WORD  bfReserved2;
	DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;


typedef struct tagBITMAPINFOHEADER {
	DWORD biSize;
	LONG  biWidth;
	LONG  biHeight;
	WORD  biPlanes;
	WORD  biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG  biXPelsPerMeter;
	LONG  biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

void saveBMP(struct SwsContext *img_convert_ctx, AVFrame *frame, char *filename)
{
	//1 先进行转换,  YUV420=>RGB24:
	int w = frame->width;
	int h = frame->height;


	int numBytes = avpicture_get_size(AV_PIX_FMT_BGR24, w, h);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));


	AVFrame *pFrameRGB = av_frame_alloc();
	/* buffer is going to be written to rawvideo file, no alignment */
   /*
   if (av_image_alloc(pFrameRGB->data, pFrameRGB->linesize,
							 w, h, AV_PIX_FMT_BGR24, pix_fmt, 1) < 0) {
	   fprintf(stderr, "Could not allocate destination image\n");
	   exit(1);
   }
   */
	avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_BGR24, w, h);

	sws_scale(img_convert_ctx, frame->data, frame->linesize,
		0, h, pFrameRGB->data, pFrameRGB->linesize);

	//2 构造 BITMAPINFOHEADER
	BITMAPINFOHEADER header;
	header.biSize = sizeof(BITMAPINFOHEADER);


	header.biWidth = w;
	header.biHeight = h * (-1);
	header.biBitCount = 24;
	header.biCompression = 0;
	header.biSizeImage = 0;
	header.biClrImportant = 0;
	header.biClrUsed = 0;
	header.biXPelsPerMeter = 0;
	header.biYPelsPerMeter = 0;
	header.biPlanes = 1;

	//3 构造文件头
	BITMAPFILEHEADER bmpFileHeader = { 0, };
	//HANDLE hFile = NULL;
	DWORD dwTotalWriten = 0;
	DWORD dwWriten;

	bmpFileHeader.bfType = 0x4d42; //'BM';
	bmpFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + numBytes;
	bmpFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	FILE* pf = fopen(filename, "wb");
	fwrite(&bmpFileHeader, sizeof(BITMAPFILEHEADER), 1, pf);
	fwrite(&header, sizeof(BITMAPINFOHEADER), 1, pf);
	fwrite(pFrameRGB->data[0], 1, numBytes, pf);
	fclose(pf);


	//释放资源
	//av_free(buffer);
	av_freep(&pFrameRGB[0]);
	av_free(pFrameRGB);
}

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
	char *filename)
{
	FILE *f;
	int i;

	f = fopen(filename, "w");
	fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
	for (i = 0; i < ysize; i++)
		fwrite(buf + i * wrap, 1, xsize, f);
	fclose(f);
}

static int decode_write_frame(const char *outfilename, AVCodecContext *avctx,
	struct SwsContext *img_convert_ctx, AVFrame *frame, int *frame_count, AVPacket *pkt, int last)
{
	int len, got_frame;
	char buf[1024];
	//获取解码后的帧
	len = avcodec_decode_video2(avctx, frame, &got_frame, pkt);
	if (len < 0) {
		fprintf(stderr, "Error while decoding frame %d\n", *frame_count);
		return len;
	}
	if (got_frame) {
		printf("Saving %sframe %3d\n", last ? "last " : "", *frame_count);
		fflush(stdout);

		/* the picture is allocated by the decoder, no need to free it */
		snprintf(buf, sizeof(buf), "%s-%d.bmp", outfilename, *frame_count);

		/*
		pgm_save(frame->data[0], frame->linesize[0],
				 frame->width, frame->height, buf);
		*/
		//转成rgb数据
		saveBMP(img_convert_ctx, frame, buf);

		(*frame_count)++;
	}
	if (pkt->data) {
		pkt->size -= len;
		pkt->data += len;
	}
	return 0;
}


int ffmpeg_in_action::decode_video()
{
	/*
	结构
	AVCodec 编码器结构体
	AVCodecContext 编码器上下文
	AVFrame 解码后的帧

	#AVFrame的申请于释放
	av_frame_alloc/av_frame_free()

	#AVCodecContext上下文的分配与释放
	avcodec_alloc_context3()/avcodec_free_context()

	解码步骤
	1.查找解码器（avcodec_find_decoder）
	2.打开解码器(avcodec_open2)
	3.解码(avcodec_decode_video2)

	*/


	int ret;

	FILE *f;

	const char *filename, *outfilename;

	AVFormatContext *fmt_ctx = NULL;

	const AVCodec *codec;
	AVCodecContext *c = NULL;

	AVStream *st = NULL;
	int stream_index;

	int frame_count;
	AVFrame *frame;

	struct SwsContext *img_convert_ctx;

	//uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
	AVPacket avpkt;
	filename = "D:\\test.mp4";
	outfilename = "D:\\test_bmp\\";

	/* register all formats and codecs */
	av_register_all();

	/* open input file, and allocate format context */
	if (avformat_open_input(&fmt_ctx, filename, NULL, NULL) < 0) {
		fprintf(stderr, "Could not open source file %s\n", filename);
		exit(1);
	}

	/* retrieve stream information */
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Could not find stream information\n");
		exit(1);
	}

	/* dump input information to stderr */
	av_dump_format(fmt_ctx, 0, filename, 0);

	av_init_packet(&avpkt);

	/* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
	//memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
	//获取关键帧

	ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not find %s stream in input file '%s'\n",
			av_get_media_type_string(AVMEDIA_TYPE_VIDEO), filename);
		return ret;
	}

	stream_index = ret;
	st = fmt_ctx->streams[stream_index];

	/* find decoder for the stream 查找编解码器*/
	codec = avcodec_find_decoder(st->codecpar->codec_id);
	if (!codec) {
		fprintf(stderr, "Failed to find %s codec\n",
			av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		return AVERROR(EINVAL);
	}

	//分配编码器的上下文（输出）
	c = avcodec_alloc_context3(NULL);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}

	/* Copy codec parameters from input stream to output codec context */
	if ((ret = avcodec_parameters_to_context(c, st->codecpar)) < 0) {
		fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
			av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		return ret;
	}


	/*
	if (codec->capabilities & AV_CODEC_CAP_TRUNCATED)
		c->flags |= AV_CODEC_FLAG_TRUNCATED; // we do not send complete frames
	*/

	/* For some codecs, such as msmpeg4 and mpeg4, width and height
	   MUST be initialized there because this information is not
	   available in the bitstream. */

	   /* open it 打开解码器 */
	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}

	/*
	f = fopen(filename, "rb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", filename);
		exit(1);
	}
	*/

	img_convert_ctx = sws_getContext(c->width, c->height,
		c->pix_fmt,
		c->width, c->height,
		AV_PIX_FMT_RGB24,
		SWS_BICUBIC, NULL, NULL, NULL);

	if (img_convert_ctx == NULL)
	{
		fprintf(stderr, "Cannot initialize the conversion context\n");
		exit(1);
	}

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}

	frame_count = 0;
	while (av_read_frame(fmt_ctx, &avpkt) >= 0) {
		/*
		avpkt.size = fread(inbuf, 1, INBUF_SIZE, f);
		if (avpkt.size == 0)
			break;
		*/

		/* NOTE1: some codecs are stream based (mpegvideo, mpegaudio)
		   and this is the only method to use them because you cannot
		   know the compressed data size before analysing it.

		   BUT some other codecs (msmpeg4, mpeg4) are inherently frame
		   based, so you must call them with all the data for one
		   frame exactly. You must also initialize 'width' and
		   'height' before initializing them. */

		   /* NOTE2: some codecs allow the raw parameters (frame size,
			  sample rate) to be changed at any frame. We handle this, so
			  you should also take care of it */

			  /* here, we use a stream based decoder (mpeg1video), so we
				 feed decoder and see if it could decode a frame */
				 //avpkt.data = inbuf;
				 //while (avpkt.size > 0)
		if (avpkt.stream_index == stream_index) {
			if (decode_write_frame(outfilename, c, img_convert_ctx, frame, &frame_count, &avpkt, 0) < 0)
				exit(1);
		}

		av_packet_unref(&avpkt);
	}

	/* Some codecs, such as MPEG, transmit the I- and P-frame with a
	   latency of one frame. You must do the following to have a
	   chance to get the last frame of the video. */
	avpkt.data = NULL;
	avpkt.size = 0;
	decode_write_frame(outfilename, c, img_convert_ctx, frame, &frame_count, &avpkt, 1);

	fclose(f);

	avformat_close_input(&fmt_ctx);

	sws_freeContext(img_convert_ctx);
	avcodec_free_context(&c);
	av_frame_free(&frame);

	return 0;
}



int ffmpeg_in_action::encode_video()
{
	/*
	//查找编码器
	avcodec_find_encoder_by_name()
	//设置编码参数、打开编码器
	acodec_opens()
	//编码
	avcodec_encode_video2
	*/
	const char *filename, *codec_name;
	const AVCodec *codec;
	AVCodecContext *c = NULL;
	int i, ret, x, y, got_output;
	FILE *f;
	AVFrame *frame;
	AVPacket pkt;
	uint8_t endcode[] = { 0, 0, 1, 0xb7 };


	filename = "D:\\encode1.h264";
	codec_name = "libx264";
	
	avcodec_register_all();

	/* find the mpeg1video encoder */
	codec = avcodec_find_encoder_by_name(codec_name);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}
	//创建上下文
	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}

	/* put sample parameters */
	c->bit_rate = 400000;
	/* resolution must be a multiple of two */
	c->width = 352;
	c->height = 288;
	/* frames per second */
	AVRational time_base;
	AVRational frame_rate;
	time_base.den = 1;
	time_base.num = 25;
	frame_rate.den = 25;
	frame_rate.num = 1;
	c->time_base = time_base;
	c->framerate = frame_rate;

	/* emit one intra frame every ten frames
	 * check frame pict_type before passing frame
	 * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
	 * then gop_size is ignored and the output of encoder
	 * will always be I frame irrespective to gop_size
	 */
	//一组帧多少（只有一个关键帧）
	c->gop_size = 10;
	c->max_b_frames = 1;
	c->pix_fmt = AV_PIX_FMT_YUV420P;

	//预先设置好的参数，压缩速度慢
	if (codec->id == AV_CODEC_ID_H264)
		av_opt_set(c->priv_data, "preset", "slow", 0);

	/* open it */
	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}

	f = fopen(filename, "wb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", filename);
		exit(1);
	}

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}
	frame->format = c->pix_fmt;
	frame->width = c->width;
	frame->height = c->height;

	ret = av_frame_get_buffer(frame, 32);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate the video frame data\n");
		exit(1);
	}

	/* encode 1 second of video */
	for (i = 0; i < 25; i++) {
		av_init_packet(&pkt);
		pkt.data = NULL;    // packet data will be allocated by the encoder
		pkt.size = 0;

		fflush(stdout);

		/* make sure the frame data is writable */
		ret = av_frame_make_writable(frame);
		if (ret < 0)
			exit(1);

		/* prepare a dummy image */
		/* Y */
		for (y = 0; y < c->height; y++) {
			for (x = 0; x < c->width; x++) {
				frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
			}
		}

		/* Cb and Cr */
		for (y = 0; y < c->height / 2; y++) {
			for (x = 0; x < c->width / 2; x++) {
				frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
				frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
			}
		}

		frame->pts = i;

		/* encode the image
		got_output判断是否压缩成功
		*/
		ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}

		if (got_output) {
			printf("Write frame %3d (size=%5d)\n", i, pkt.size);
			fwrite(pkt.data, 1, pkt.size, f);
			av_packet_unref(&pkt);
		}
	}

	/* get the delayed frames */
	for (got_output = 1; got_output; i++) {
		fflush(stdout);

		ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}

		if (got_output) {
			printf("Write frame %3d (size=%5d)\n", i, pkt.size);
			fwrite(pkt.data, 1, pkt.size, f);
			av_packet_unref(&pkt);
		}
	}

	/* add sequence end code to have a real MPEG file */
	fwrite(endcode, 1, sizeof(endcode), f);
	fclose(f);

	avcodec_free_context(&c);
	av_frame_free(&frame);

	return 0;
}


#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096
#define MAX_AUDIO_FRAME_SIZE 192000
int ffmpeg_in_action::decode_audio()
{
	const char *outfilename, *filename;

	int i, ret;

	int err_code;
	char errors[1024];

	int audiostream_index = -1;

	AVFormatContext *pFormatCtx = NULL;

	const AVCodec *codec;
	AVCodecContext *c = NULL;

	int len;
	FILE *f, *outfile;
	uint8_t inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];

	AVPacket avpkt;
	AVFrame *decoded_frame = NULL;

	filename = "D:\\test_audio.aac";
	outfilename ="D:\\1234567.pcm";

	/* register all the codecs */
	av_register_all();

	av_init_packet(&avpkt);

	/* open input file, and allocate format context */
	if ((err_code = avformat_open_input(&pFormatCtx, filename, NULL, NULL)) < 0) {
		av_strerror(err_code, errors, 1024);
		fprintf(stderr, "Could not open source file %s, %d(%s)\n", filename, err_code, errors);
		return -1;
	}

	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
		return -1; // Couldn't find stream information


	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, filename, 0);

	for (i = 0; i < pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			audiostream_index = i;
		}
	}

	/* find the MPEG audio decoder */
	//codec = avcodec_find_decoder_by_name("libfdk_aac");
	//codec = avcodec_find_decoder(pFormatCtx->streams[audiostream_index]->codec->codec_id/*AV_CODEC_ID_MP2*/);
	/*
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}
	*/

	c = avcodec_alloc_context3(NULL);
	if (!c) {
		fprintf(stderr, "Could not allocate audio codec context\n");
		exit(1);
	}

	ret = avcodec_parameters_to_context(c, pFormatCtx->streams[audiostream_index]->codecpar);
	if (ret < 0) {
		return -1;
	}

	codec = avcodec_find_decoder(c->codec_id);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}

	//Out Audio Param
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;

	//AAC:1024  MP3:1152
	int out_nb_samples = c->frame_size;
	//AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;

	int out_sample_rate = 44100;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	//Out Buffer Size
	int out_buffer_size = av_samples_get_buffer_size(NULL,
		out_channels,
		out_nb_samples,
		AV_SAMPLE_FMT_S16,
		1);

	uint8_t *out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
	int64_t in_channel_layout = av_get_default_channel_layout(c->channels);

	struct SwrContext *audio_convert_ctx;
	audio_convert_ctx = swr_alloc();
	audio_convert_ctx = swr_alloc_set_opts(audio_convert_ctx,
		out_channel_layout,
		AV_SAMPLE_FMT_S16,
		out_sample_rate,
		in_channel_layout,
		c->sample_fmt,
		c->sample_rate,
		0,
		NULL);
	swr_init(audio_convert_ctx);

	/* open it */
	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}

	/*
	f = fopen(filename, "rb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", filename);
		exit(1);
	}
	*/

	outfile = fopen(outfilename, "wb");
	if (!outfile) {
		av_free(c);
		exit(1);
	}

	/* decode until eof */
	/*
	avpkt.data = inbuf;
	avpkt.size = fread(inbuf, 1, AUDIO_INBUF_SIZE, f);
	*/

	while (1) {
		int i, ch;
		int got_frame = 0;

		if (!decoded_frame) {
			if (!(decoded_frame = av_frame_alloc())) {
				fprintf(stderr, "Could not allocate audio frame\n");
				exit(1);
			}
		}

		if (av_read_frame(pFormatCtx, &avpkt) < 0) {
			if (pFormatCtx->pb->error == 0) {
				std::this_thread::sleep_for(std::chrono::seconds(100));
				//Sleep(100); /* no error; wait for user input */
				continue;
			}
			else {
				break;
			}
		}

		if (avpkt.stream_index != audiostream_index) {
			av_free_packet(&avpkt);
			continue;
		}

		len = avcodec_decode_audio4(c, decoded_frame, &got_frame, &avpkt);
		if (len < 0) {
			av_strerror(len, errors, 1024);
			fprintf(stderr, "Error while decoding, err_code:%d, err:%s\n", len, errors);
			exit(1);
		}
		if (got_frame) {
			/* if a frame has been decoded, output it */
			int data_size = av_get_bytes_per_sample(c->sample_fmt);
			if (data_size < 0) {
				/* This should not occur, checking just for paranoia */
				fprintf(stderr, "Failed to calculate data size\n");
				exit(1);
			}
			swr_convert(audio_convert_ctx,
				&out_buffer,
				MAX_AUDIO_FRAME_SIZE,
				(const uint8_t **)decoded_frame->data,
				decoded_frame->nb_samples);

			fwrite(out_buffer, 1, out_buffer_size, outfile);

			/*
			for (i=0; i<decoded_frame->nb_samples; i++)
				for (ch=0; ch<c->channels; ch++)
					fwrite(decoded_frame->data[ch] + data_size*i, 1, data_size, outfile);
			*/
		}
		avpkt.size -= len;
		avpkt.data += len;
		avpkt.dts =
			avpkt.pts = AV_NOPTS_VALUE;

		//if (avpkt.size < AUDIO_REFILL_THRESH) {
			/* Refill the input buffer, to avoid trying to decode
			 * incomplete frames. Instead of this, one could also use
			 * a parser, or use a proper container format through
			 * libavformat. */
			 /*
				 memmove(inbuf, avpkt.data, avpkt.size);
				 avpkt.data = inbuf;
				 len = fread(avpkt.data + avpkt.size, 1,
							 AUDIO_INBUF_SIZE - avpkt.size, f);
				 if (len > 0)
					 avpkt.size += len;
			 }
			 */
	}

	fclose(outfile);
	//fclose(f);

	avcodec_free_context(&c);
	av_frame_free(&decoded_frame);

	return 0;
}
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
	const enum AVSampleFormat *p = codec->sample_fmts;

	while (*p != AV_SAMPLE_FMT_NONE) {
		if (*p == sample_fmt)
			return 1;
		p++;
	}
	return 0;
}

/* just pick the highest supported samplerate */
static int select_sample_rate(const AVCodec *codec)
{
	const int *p;
	int best_samplerate = 0;

	if (!codec->supported_samplerates)
		return 44100;

	p = codec->supported_samplerates;
	while (*p) {
		if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate))
			best_samplerate = *p;
		p++;
	}
	return best_samplerate;
}

/* select layout with the highest channel count */
static int select_channel_layout(const AVCodec *codec)
{
	const uint64_t *p;
	uint64_t best_ch_layout = 0;
	int best_nb_channels = 0;

	if (!codec->channel_layouts)
		return AV_CH_LAYOUT_STEREO;

	p = codec->channel_layouts;
	while (*p) {
		int nb_channels = av_get_channel_layout_nb_channels(*p);

		if (nb_channels > best_nb_channels) {
			best_ch_layout = *p;
			best_nb_channels = nb_channels;
		}
		p++;
	}
	return best_ch_layout;
}
int ffmpeg_in_action::encode_audio()
{
	const char *filename;
	const AVCodec *codec;
	AVCodecContext *c = NULL;
	AVFrame *frame;
	AVPacket pkt;
	int i, j, k, ret, got_output;
	FILE *f;
	uint16_t *samples;
	float t, tincr;

// 	if (argc <= 1) {
// 		fprintf(stderr, "Usage: %s <output file>\n", argv[0]);
// 		return 0;
// 	}
	filename = "D:\\test_audio.aac";

	/* register all the codecs */
	avcodec_register_all();

	/* find the MP2 encoder */
	codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate audio codec context\n");
		exit(1);
	}

	/* put sample parameters */
	c->bit_rate = 64000;

	/* check that the encoder supports s16 pcm input */
	c->sample_fmt = AV_SAMPLE_FMT_S16;
	if (!check_sample_fmt(codec, c->sample_fmt)) {
		fprintf(stderr, "Encoder does not support sample format %s",
			av_get_sample_fmt_name(c->sample_fmt));
		exit(1);
	}

	/* select other audio parameters supported by the encoder */
	c->sample_rate = select_sample_rate(codec);
	c->channel_layout = select_channel_layout(codec);
	c->channels = av_get_channel_layout_nb_channels(c->channel_layout);

	/* open it */
	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}

	f = fopen(filename, "wb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", filename);
		exit(1);
	}

	/* frame containing input raw audio */
	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate audio frame\n");
		exit(1);
	}

	frame->nb_samples = c->frame_size;
	frame->format = c->sample_fmt;
	frame->channel_layout = c->channel_layout;

	/* allocate the data buffers */
	ret = av_frame_get_buffer(frame, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate audio data buffers\n");
		exit(1);
	}

	/* encode a single tone sound */
	t = 0;
	tincr = 2 * M_PI * 440.0 / c->sample_rate;
	for (i = 0; i < 200; i++) {
		av_init_packet(&pkt);
		pkt.data = NULL; // packet data will be allocated by the encoder
		pkt.size = 0;

		/* make sure the frame is writable -- makes a copy if the encoder
		 * kept a reference internally */
		ret = av_frame_make_writable(frame);
		if (ret < 0)
			exit(1);
		samples = (uint16_t*)frame->data[0];

		for (j = 0; j < c->frame_size; j++) {
			samples[2 * j] = (int)(sin(t) * 10000);

			for (k = 1; k < c->channels; k++)
				samples[2 * j + k] = samples[2 * j];
			t += tincr;
		}
		/* encode the samples */
		ret = avcodec_encode_audio2(c, &pkt, frame, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding audio frame\n");
			exit(1);
		}
		if (got_output) {
			fwrite(pkt.data, 1, pkt.size, f);
			av_packet_unref(&pkt);
		}
	}

	/* get the delayed frames */
	for (got_output = 1; got_output; i++) {
		ret = avcodec_encode_audio2(c, &pkt, NULL, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}

		if (got_output) {
			fwrite(pkt.data, 1, pkt.size, f);
			av_packet_unref(&pkt);
		}
	}
	fclose(f);

	av_frame_free(&frame);
	avcodec_free_context(&c);

	return 0;
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
//	cutFile();
	seekCutFile();
}

void ffmpeg_in_action::on_getVideo_btn_clicked()
{
	getVideoData();
}

void ffmpeg_in_action::on_mp4flv_btn_clicked()
{
	mp4ToFlv();
}

void ffmpeg_in_action::on_h264_decode_btn_clicked()
{
	decode_video();
}

void ffmpeg_in_action::on_h264_encode_btn_clicked()
{
	encode_video();
}

void ffmpeg_in_action::on_aac_encode_btn_clicked()
{
	decode_audio();
	//encode_audio();
}
