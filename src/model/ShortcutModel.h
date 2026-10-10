#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace gomp {

class Preferences;

// gomp's keyboard shortcuts: defaults, user overrides (persisted in Preferences) and conflict
// detection. QML binds `App.shortcuts.map[id]` so rebinding takes effect immediately.
class ShortcutModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(QVariantMap map READ map NOTIFY changed)
    Q_PROPERTY(int conflictCount READ conflictCount NOTIFY changed)
public:
    enum Role { IdRole = Qt::UserRole + 1, TitleRole, CategoryRole, SequenceRole, DefaultRole, IsDefaultRole, ConflictRole };

    explicit ShortcutModel(Preferences *prefs, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QVariantMap map() const;
    [[nodiscard]] int conflictCount() const;
    Q_INVOKABLE QString sequence(const QString &id) const;
    Q_INVOKABLE QString conflictFor(const QString &id, const QString &sequence) const; // title of clashing action
    // Returns empty on success, or a message when the sequence is unusable.
    Q_INVOKABLE QString setSequence(const QString &id, const QString &sequence);
    Q_INVOKABLE void reset(const QString &id);
    Q_INVOKABLE void resetAll();
    Q_INVOKABLE static QString portableText(const QString &sequence); // "Ctrl+Shift+K" -> "Ctrl Shift K" glyph-ready

    struct Def {
        QString id;
        QString title;
        QString category;
        QString sequence;
    };
    static const QList<Def> &defaults();

signals:
    void changed();

private:
    [[nodiscard]] QString current(const Def &d) const;
    Preferences *m_prefs;
};

} // namespace gomp
