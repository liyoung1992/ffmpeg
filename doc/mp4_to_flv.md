#MP4ת��flv
```
//mp4-->flv
	//���������
	//avformat_alloc_output_context2()

	//�ͷ����������
	//avform_free_context()

	//�½��µ���
	//avformat_new_stream
	//���Ʋ���
	//accodec_parameters_copy

	//д��ý���ļ�ͷ
	//avformat_write_header
	//д�ļ�����
	//av_write_frame
	//av_interleaved_wirte_frame������д�룩
	//дβ��
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
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) { //���������ļ�������
		fprintf(stderr, "Could not open input file %s", in_filename);
		goto end;
	}

	/**avformat_find_stream_info() �ú������Զ�ȡһ��������Ƶ���ݲ��һ��һЩ��ص���Ϣ ̽���ļ���Ϣ*/
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		fprintf(stderr, "Failed to retrieve input stream information");
		goto end;
	}
	av_dump_format(ifmt_ctx, 0, in_filename, 0);

	//��������ļ�������
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	if (!ofmt_ctx) {
		fprintf(stderr, "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}

	stream_mapping_size = ifmt_ctx->nb_streams;
	stream_mapping = (int *)av_mallocz_array(stream_mapping_size, sizeof(*stream_mapping));
	//����ռ�
	if (!stream_mapping) {
		ret = AVERROR(ENOMEM);
		goto end;
	}

	//AVOutputFormat ����˵���Ϣ ��FFmpeg�⸴��(���װ)�õĽṹ�壬���磬����ĵ�Э�飬����ı������
	ofmt = ofmt_ctx->oformat;

	//���ǰ�AVStream ��ȡ���ڴ���
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		AVStream *out_stream;
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVCodecParameters *in_codecpar = in_stream->codecpar; //AVCodecParameters ���ڼ�¼����������Ϣ����ͨ���д洢�����ı�����Ϣ

		if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
			in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
			in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
			stream_mapping[i] = -1;
			continue;
		}
		stream_mapping[i] = stream_index++;

		//��AVFormatContext�д���Streamͨ�������ڼ�¼ͨ����Ϣ
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
		 * ���ڴ�FFmpeg����������ļ�
		 * ����1:�������óɹ�֮�󴴽���AVIOContext�ṹ��
		 * ����2:�������Э��ĵ�ַ(�ļ�·��)
		 * ����3:�򿪵�ַ�ķ�ʽ   AVIO_FLAG_READ ֻ��  AVIO_FLAG_WRITE ֻд  AVIO_FLAG_READ_WRITE ��д
		 *
		 * ����:
		 * avio_open2() �ڲ���Ҫ������������:ffurl_open() ��ffio_fdopen(), ����ffurl_open()���ڳ�ʼ��URLContext,ffio_fdopen()���ڸ���URLContext��ʼ��AVIOContext. URLContext�а�����URLProtocol����˾����Э���д�ȹ�����AVIOContext������URLContext�Ķ�д�����������һ��'��װ'(ͨ��retry_transfer_wrapper()����)
		 * URLProtocol ��Ҫ��������Э���д�ĺ���ָ�� url_open() url_read() url_write() url_close
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

	//ȥ�ڴ���ȡ
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
		log_packet(ifmt_ctx, &pkt, "in"); //��ӡ

		/**��ͬʱ�������
		 * av_rescale_q(a, b, c)
		 * av_rescale_q_rnd(a, b, c, AVRoundion rnd) //AVRoundion ����ȡ���ķ�ʽ
		 * ����:
			��ʱ�����һ��ʱ������������һ��ʱ��ʱ���õĺ��������У�a��ʾҪ�����ֵ��b��ʽԭ����ʱ�����c��ʾҪת����ʱ����� ����㹫ʽ�� a * b / c
		 *
		 * ʱ���ת��
		 *  time_in_seconds = av_q2q(AV_TIME_BASE_Q) * timestamp
		 *
		 * ��תʱ���
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
	avformat_close_input(&ifmt_ctx); //�ر������ļ�������

	if (ofmt_ctx && !(ofmt->flags && AVFMT_NOFILE)) {
		avio_closep(&ofmt_ctx->pb);
	}

	//�ͷ�����ļ���������
	avformat_free_context(ofmt_ctx);

	av_freep(&stream_mapping);

	if (ret < 0 && ret != AVERROR_EOF) {
		fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
		return -1;
	}

	return 0;

```