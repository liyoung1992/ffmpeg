# 音频编解码
```

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
```