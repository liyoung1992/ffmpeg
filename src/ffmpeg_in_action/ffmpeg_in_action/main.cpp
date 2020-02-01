#include "ffmpeg_in_action.h"
#include <QtWidgets/QApplication>
#include "../../helloworld/helloworld.h"

int main(int argc, char *argv[])
{
	//test ffmpeg
	helloWorld();

	QApplication a(argc, argv);
	ffmpeg_in_action w;
	w.show();
	return a.exec();
}
