#include "ShortcutModel.h"

#include "app/Preferences.h"

#include <QKeySequence>

using namespace Qt::StringLiterals;

namespace gomp {

const QList<ShortcutModel::Def> &ShortcutModel::defaults()
{
    static const QList<Def> defs{
        {u"palette.open"_s, u"Open command palette"_s, u"General"_s, u"Ctrl+K"_s},
        {u"help.shortcuts"_s, u"Show keyboard shortcuts"_s, u"General"_s, u"Ctrl+/"_s},
        {u"app.quit"_s, u"Quit gomp"_s, u"General"_s, u"Ctrl+Q"_s},
        {u"nav.agent"_s, u"Go to Agent"_s, u"Navigation"_s, u"Ctrl+1"_s},
        {u"nav.sessions"_s, u"Go to Sessions"_s, u"Navigation"_s, u"Ctrl+2"_s},
        {u"nav.subagents"_s, u"Go to Subagents"_s, u"Navigation"_s, u"Ctrl+3"_s},
        {u"nav.settings"_s, u"Open Settings"_s, u"Navigation"_s, u"Ctrl+,"_s},
        {u"inspector.toggle"_s, u"Show or hide the inspector"_s, u"Navigation"_s, u"Ctrl+I"_s},
        {u"workspace.new"_s, u"Open a project in a new workspace"_s, u"Workspaces"_s, u"Ctrl+T"_s},
        {u"workspace.close"_s, u"Close workspace"_s, u"Workspaces"_s, u"Ctrl+W"_s},
        {u"workspace.next"_s, u"Next workspace"_s, u"Workspaces"_s, u"Ctrl+PgDown"_s},
        {u"workspace.previous"_s, u"Previous workspace"_s, u"Workspaces"_s, u"Ctrl+PgUp"_s},
        {u"composer.focus"_s, u"Focus the composer"_s, u"Agent"_s, u"Ctrl+L"_s},
        {u"agent.stop"_s, u"Stop the agent"_s, u"Agent"_s, u"Ctrl+."_s},
        {u"approval.approve"_s, u"Approve the oldest pending tool call"_s, u"Agent"_s, u"Ctrl+Return"_s},
        {u"approval.deny"_s, u"Deny the oldest pending tool call"_s, u"Agent"_s, u"Ctrl+Shift+Backspace"_s},
        {u"mode.plan"_s, u"Toggle plan mode"_s, u"Agent"_s, u"Alt+Shift+P"_s},
        {u"model.pick"_s, u"Choose a model"_s, u"Agent"_s, u"Ctrl+P"_s},
        {u"thinking.cycle"_s, u"Cycle reasoning effort"_s, u"Agent"_s, u"Ctrl+Shift+E"_s},
        {u"session.compact"_s, u"Compact the conversation"_s, u"Session"_s, u"Ctrl+Shift+K"_s},
        {u"session.fork"_s, u"Fork the session"_s, u"Session"_s, u"Ctrl+Shift+F"_s},
        {u"session.rename"_s, u"Rename the session"_s, u"Session"_s, u"F2"_s},
        {u"session.new"_s, u"Start a new session in this workspace"_s, u"Session"_s, u"Ctrl+N"_s},
    };
    return defs;
}

ShortcutModel::ShortcutModel(Preferences *prefs, QObject *parent)
    : QAbstractListModel(parent)
    , m_prefs(prefs)
{
}

QString ShortcutModel::current(const Def &d) const
{
    const auto overrides = m_prefs->shortcutOverrides();
    return overrides.contains(d.id) ? overrides.value(d.id).toString() : d.sequence;
}

int ShortcutModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(defaults().size());
}

QHash<int, QByteArray> ShortcutModel::roleNames() const
{
    return {{IdRole, "actionId"},   {TitleRole, "title"},           {CategoryRole, "category"},
            {SequenceRole, "sequence"}, {DefaultRole, "defaultSequence"}, {IsDefaultRole, "isDefault"},
            {ConflictRole, "conflict"}};
}

QVariant ShortcutModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= defaults().size())
        return {};
    const auto &d = defaults().at(index.row());
    switch (role) {
    case IdRole:
        return d.id;
    case TitleRole:
        return d.title;
    case CategoryRole:
        return d.category;
    case SequenceRole:
        return current(d);
    case DefaultRole:
        return d.sequence;
    case IsDefaultRole:
        return current(d) == d.sequence;
    case ConflictRole:
        return conflictFor(d.id, current(d));
    default:
        return {};
    }
}

QVariantMap ShortcutModel::map() const
{
    QVariantMap out;
    for (const auto &d : defaults())
        out.insert(d.id, current(d));
    return out;
}

QString ShortcutModel::sequence(const QString &id) const
{
    for (const auto &d : defaults()) {
        if (d.id == id)
            return current(d);
    }
    return {};
}

QString ShortcutModel::conflictFor(const QString &id, const QString &sequence) const
{
    if (sequence.isEmpty())
        return {};
    const QKeySequence wanted(sequence);
    for (const auto &d : defaults()) {
        if (d.id != id && QKeySequence(current(d)) == wanted)
            return d.title;
    }
    return {};
}

int ShortcutModel::conflictCount() const
{
    int n = 0;
    for (const auto &d : defaults())
        n += conflictFor(d.id, current(d)).isEmpty() ? 0 : 1;
    return n / 2 + n % 2;
}

QString ShortcutModel::setSequence(const QString &id, const QString &sequence)
{
    const QKeySequence seq(sequence);
    if (!sequence.isEmpty() && seq.isEmpty())
        return u"That key combination cannot be used."_s;
    // Text-editing keys without a modifier would make the composer unusable.
    if (seq.count() == 1) {
        const auto combo = seq[0];
        const auto mods = combo.keyboardModifiers() & ~Qt::KeypadModifier;
        const auto key = combo.key();
        const bool fKey = key >= Qt::Key_F1 && key <= Qt::Key_F35;
        if ((mods == Qt::NoModifier || mods == Qt::ShiftModifier) && !fKey)
            return u"Add Ctrl or Alt so the shortcut does not interfere with typing."_s;
    }
    auto overrides = m_prefs->shortcutOverrides();
    const auto canonical = seq.toString(QKeySequence::PortableText);
    for (const auto &d : defaults()) {
        if (d.id != id)
            continue;
        if (canonical == d.sequence)
            overrides.remove(id);
        else
            overrides.insert(id, canonical);
    }
    m_prefs->setShortcutOverrides(overrides);
    emit dataChanged(index(0), index(int(defaults().size()) - 1));
    emit changed();
    return {};
}

QVariantList ShortcutModel::entries(const QString &category) const
{
    QVariantList out;
    for (const auto &d : defaults()) {
        if (d.category == category)
            out << QVariantMap{{u"id"_s, d.id}, {u"title"_s, d.title}, {u"sequence"_s, current(d)}};
    }
    return out;
}

void ShortcutModel::reset(const QString &id)
{
    auto overrides = m_prefs->shortcutOverrides();
    overrides.remove(id);
    m_prefs->setShortcutOverrides(overrides);
    emit dataChanged(index(0), index(int(defaults().size()) - 1));
    emit changed();
}

void ShortcutModel::resetAll()
{
    m_prefs->setShortcutOverrides({});
    emit dataChanged(index(0), index(int(defaults().size()) - 1));
    emit changed();
}

QString ShortcutModel::portableText(const QString &sequence)
{
    return QKeySequence(sequence).toString(QKeySequence::NativeText);
}

} // namespace gomp
