# 文件裁剪

```

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

```