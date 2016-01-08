#include "qtquick2applicationviewer.h"
#include <QQmlContext>
#include <QtGui/QGuiApplication>

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);

  QtQuick2ApplicationViewer viewer;

  viewer.addImportPath(QStringLiteral("plugins"));

  viewer.setMainQmlFile(QStringLiteral("qml/Bananas/main.qml"));
  viewer.showExpanded();

  return app.exec();
}
