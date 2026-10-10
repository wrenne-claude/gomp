#include "AppController.h"
#include "LaunchOptions.h"
#include "util/IconProvider.h"

#include <QCommandLineParser>
#include <QDirIterator>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlEngineExtensionPlugin>
#include <QQuickStyle>
#include <QQuickWindow>

Q_IMPORT_QML_PLUGIN(Gomp_CorePlugin)

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(u"gomp"_s);
    QGuiApplication::setOrganizationName(u"gomp"_s);
    QGuiApplication::setApplicationVersion(QStringLiteral(GOMP_VERSION));
    // Wayland app id: matches the .desktop file so the compositor finds the icon and name.
    QGuiApplication::setDesktopFileName(u"dev.gomp.Gomp"_s);
    QGuiApplication::setWindowIcon(QIcon(u":/qt/qml/Gomp/icons/gomp-mark.svg"_s));

    QCommandLineParser parser;
    parser.setApplicationDescription(u"A native flight deck for the omp coding agent."_s);
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption mockOpt(u"mock"_s, u"Run against the scripted mock backend instead of omp."_s);
    const QCommandLineOption shotsOpt(u"screenshots"_s, u"Render every screen to PNGs in <dir> and exit."_s, u"dir"_s);
    const QCommandLineOption projectOpt(u"project"_s, u"Open <dir> in a workspace on start."_s, u"dir"_s);
    const QCommandLineOption sizeOpt(u"size"_s, u"Initial window size, e.g. 2560x720."_s, u"WxH"_s);
    const QCommandLineOption themeOpt(u"theme"_s, u"Force the dark or light theme."_s, u"dark|light"_s);
    const QCommandLineOption scenarioOpt(u"scenario"_s, u"Mock scenario to play on start."_s, u"name"_s);
    const QCommandLineOption reduceOpt(u"reduce-effects"_s, u"Start with blur, shaders and particles off."_s);
    parser.addOptions({mockOpt, shotsOpt, projectOpt, sizeOpt, themeOpt, scenarioOpt, reduceOpt});
    parser.process(app);

    // Our own control library replaces the style entirely; Basic is the neutral base it builds on.
    QQuickStyle::setStyle(u"Basic"_s);

    QDirIterator fonts(u":/qt/qml/Gomp/fonts"_s, {u"*.ttf"_s}, QDir::Files);
    while (fonts.hasNext())
        QFontDatabase::addApplicationFont(fonts.next());
    QFont base(u"Barlow"_s);
    base.setPixelSize(14);
    base.setHintingPreference(QFont::PreferVerticalHinting);
    QGuiApplication::setFont(base);

    QSize size;
    if (const auto parts = parser.value(sizeOpt).split(u'x'); parts.size() == 2)
        size = QSize(parts.at(0).toInt(), parts.at(1).toInt());
    gomp::LaunchOptionsObject::configure(size, parser.value(themeOpt), parser.value(scenarioOpt), parser.isSet(reduceOpt));

    gomp::AppController::Options options;
    options.mock = parser.isSet(mockOpt) || parser.isSet(shotsOpt);
    options.screenshotDir = parser.value(shotsOpt);
    options.openProject = parser.value(projectOpt);
    gomp::AppController controller(options);

    QQmlApplicationEngine engine;
    engine.addImageProvider(u"icons"_s, new gomp::IconProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule(u"Gomp"_s, u"Main"_s);
    if (engine.rootObjects().isEmpty())
        return 1;
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()))
        controller.effects()->attach(window);
    controller.start();
    return QGuiApplication::exec();
}
