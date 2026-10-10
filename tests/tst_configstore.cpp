#include "adapter/OmpCli.h"
#include "config/ConfigStore.h"
#include "config/McpStore.h"
#include "config/SettingsSchema.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace gomp;
using namespace Qt::StringLiterals;

// A stand-in omp: answers `config list --json` and records `config set|reset` calls, so the
// store is exercised end to end without touching a real configuration.
static const char *FakeOmp = R"(#!/bin/sh
log="$GOMP_FAKE_LOG"
if [ "$1" = "config" ] && [ "$2" = "list" ]; then
  echo '{"tools.approvalMode":{"value":"yolo","type":"enum"},"task.maxConcurrency":{"value":16,"type":"number"},"brandNew.key":{"type":"boolean","description":"From a newer omp"}}'
  exit 0
fi
if [ "$1" = "config" ] && [ "$2" = "set" ]; then
  echo "set $3 $4" >> "$log"
  if [ "$4" = "bogus" ]; then echo "Error: Invalid value: bogus" >&2; exit 1; fi
  exit 0
fi
if [ "$1" = "config" ] && [ "$2" = "reset" ]; then echo "reset $3" >> "$log"; exit 0; fi
exit 2
)";

class TestConfigStore : public QObject
{
    Q_OBJECT
    QTemporaryDir home;
    QString fake;
    QString log;

    void write(const QString &path, const QByteArray &data)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }

private slots:
    void initTestCase()
    {
        fake = home.filePath(u"omp"_s);
        write(fake, FakeOmp);
        QFile::setPermissions(fake, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        log = home.filePath(u"calls.log"_s);
        qputenv("GOMP_FAKE_LOG", log.toLocal8Bit());
        qputenv("PI_CONFIG_DIR", home.filePath(u"agent"_s).toLocal8Bit());
        qputenv("XDG_STATE_HOME", home.filePath(u"state"_s).toLocal8Bit());
        write(home.filePath(u"agent/config.yml"_s), "task:\n  maxConcurrency: 16\nunknownByGomp: keep-me\n");
    }

    void validation()
    {
        SettingsSchema schema;
        QVERIFY(schema.loadBundled());
        const auto *mode = schema.find(u"tools.approvalMode"_s);
        QVERIFY(mode);
        QVERIFY(SettingsSchema::validate(*mode, u"write"_s).isEmpty());
        QVERIFY(!SettingsSchema::validate(*mode, u"sometimes"_s).isEmpty());
        const auto *conc = schema.find(u"task.maxConcurrency"_s);
        QVERIFY(conc);
        QVERIFY(!SettingsSchema::validate(*conc, u"many"_s).isEmpty());
        QVERIFY(SettingsSchema::validate(*conc, 4).isEmpty());
    }

    void stageReviewApply()
    {
        SettingsSchema schema;
        schema.loadBundled();
        OmpCli cli;
        cli.setProgram(fake);
        ConfigStore store(&cli, &schema);
        QSignalSpy loaded(&store, &ConfigStore::loaded);
        store.reload();
        QVERIFY(loaded.wait(5000));
        QVERIFY(schema.find(u"brandNew.key"_s)); // live keys merge into the schema

        QVERIFY(!store.stage(u"tools.approvalMode"_s, u"never"_s, ConfigStore::Scope::Global).isEmpty());
        QCOMPARE(store.stagedCount(), 0);
        QVERIFY(store.stage(u"tools.approvalMode"_s, u"write"_s, ConfigStore::Scope::Global).isEmpty());
        QVERIFY(store.stage(u"task.maxConcurrency"_s, 4, ConfigStore::Scope::Global).isEmpty());
        QCOMPARE(store.stagedCount(), 2);
        QCOMPARE(store.scopedValue(u"task.maxConcurrency"_s, ConfigStore::Scope::Global).toDouble(), 4.0);

        const auto preview = store.preview();
        QCOMPARE(preview.size(), 1);
        const auto unified = diff::toUnified(preview.first().lines);
        QVERIFY2(unified.contains(u"-  maxConcurrency: 16"_s), qPrintable(unified));
        QVERIFY2(unified.contains(u"+  maxConcurrency: 4"_s), qPrintable(unified));
        QVERIFY(unified.contains(u"approvalMode: write"_s));

        QSignalSpy applied(&store, &ConfigStore::applied);
        store.applyStaged();
        QVERIFY(applied.wait(5000));
        QVERIFY(applied.first().at(0).toBool());
        QFile calls(log);
        QVERIFY(calls.open(QIODevice::ReadOnly));
        const auto text = QString::fromUtf8(calls.readAll());
        QVERIFY(text.contains(u"set tools.approvalMode write"_s));
        QVERIFY(text.contains(u"set task.maxConcurrency 4"_s));
        QVERIFY(!QDir(ConfigStore::backupDir()).entryList({u"*config*"_s}).isEmpty()); // backed up first
    }

    void rejectedValueIsReported()
    {
        SettingsSchema schema;
        schema.loadBundled();
        OmpCli cli;
        cli.setProgram(fake);
        ConfigStore store(&cli, &schema);
        // "bogus" passes gomp's string check for a string setting but omp rejects it.
        const auto *anyString = [&]() -> const SettingDef * {
            for (const auto &d : schema.settings())
                if (d.type == "string"_L1) return &d;
            return nullptr;
        }();
        QVERIFY(anyString);
        QVERIFY(store.stage(anyString->id, u"bogus"_s, ConfigStore::Scope::Global).isEmpty());
        QSignalSpy applied(&store, &ConfigStore::applied);
        store.applyStaged();
        QVERIFY(applied.wait(5000));
        QVERIFY(!applied.first().at(0).toBool());
        QVERIFY(applied.first().at(1).toString().contains(u"Invalid value"_s));
    }

    void projectScopeWritesOverlayAndKeepsUnknownKeys()
    {
        SettingsSchema schema;
        schema.loadBundled();
        OmpCli cli;
        cli.setProgram(fake);
        ConfigStore store(&cli, &schema);
        const auto project = home.filePath(u"proj"_s);
        write(project + u"/.omp/config.yml"_s, "custom:\n  thing: 1\n");
        store.setProjectDir(project);
        QVERIFY(store.stage(u"task.maxConcurrency"_s, 2, ConfigStore::Scope::Project).isEmpty());
        QSignalSpy applied(&store, &ConfigStore::applied);
        store.applyStaged();
        QVERIFY(applied.count() == 1 || applied.wait(2000));
        QFile f(project + u"/.omp/config.yml"_s);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto text = QString::fromUtf8(f.readAll());
        QVERIFY2(text.contains(u"custom:\n  thing: 1"_s), qPrintable(text));
        QVERIFY2(text.contains(u"maxConcurrency: 2"_s), qPrintable(text));
        QVERIFY(store.isSetIn(ConfigStore::Scope::Project, u"task.maxConcurrency"_s));
        QCOMPARE(store.scopedValue(u"task.maxConcurrency"_s, ConfigStore::Scope::Project).toDouble(), 2.0);
    }

    void rawValidation()
    {
        SettingsSchema schema;
        schema.loadBundled();
        OmpCli cli;
        ConfigStore store(&cli, &schema);
        QVERIFY(store.validateRaw(u"tools:\n  approvalMode: write\nwhatever: 1\n"_s).isEmpty());
        QVERIFY(!store.validateRaw(u"tools:\n  approvalMode: sometimes\n"_s).isEmpty());
        QVERIFY(!store.validateRaw(u"a: &x 1\n"_s).isEmpty());
    }

    void mcpValidation()
    {
        QVERIFY(McpStore::validateServer(u"docs"_s, {{"command"_L1, u"npx"_s}}).isEmpty());
        QVERIFY(!McpStore::validateServer(u"has space"_s, {{"command"_L1, u"npx"_s}}).isEmpty());
        QVERIFY(!McpStore::validateServer(u"docs"_s, {{"type"_L1, u"http"_s}, {"url"_L1, u"notaurl"_s}}).isEmpty());
        QVERIFY(McpStore::validateServer(u"docs"_s, {{"type"_L1, u"http"_s}, {"url"_L1, u"https://mcp.example.com/mcp"_s}}).isEmpty());
        QVERIFY(!McpStore::validateServer(u"docs"_s, {{"command"_L1, u"x"_s}, {"url"_L1, u"https://e.com"_s}}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestConfigStore)
#include "tst_configstore.moc"
