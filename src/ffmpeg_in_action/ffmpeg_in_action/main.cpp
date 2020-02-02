#include "ffmpeg_in_action.h"
#include <QtWidgets/QApplication>
#include "../../helloworld/helloworld.h"
#include "../../ffmpeg_sdl_player/player.h"

int main(int argc, char *argv[])
{
	//test ffmpeg
	helloWorld();
	//player
	PalyerDemo();

	QApplication a(argc, argv);
	ffmpeg_in_action w;
	w.show();
	return a.exec();
}
