#include "gui/MessengerWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
	QApplication app(argc, argv);
	MessengerWindow window;
	window.show();
	return app.exec();
}

