#include "notebookmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

NotebookManager::NotebookManager(QObject *parent)
    : QAbstractListModel(parent) {
    // Notebooks live in ~/Documents
    const QString docsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    m_notebooksRoot = docsDir;

    loadLastOpened();

    // Watch the root and its direct children for external changes
    if (!m_notebooksRoot.isEmpty() && QDir(m_notebooksRoot).exists()) {
        refreshInternal();
        m_watcher.addPath(m_notebooksRoot);
    }

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &NotebookManager::refreshInternal);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &NotebookManager::refreshInternal);
}

void NotebookManager::setNotebooksRoot(const QString &root) {
    if (m_notebooksRoot == root)
        return;

    // Stop watching old root
    const QStringList dirs = m_watcher.directories();
    if (!dirs.isEmpty())
        m_watcher.removePaths(dirs);

    m_notebooksRoot = root;
    QDir().mkpath(m_notebooksRoot);
    if (QDir(m_notebooksRoot).exists())
        m_watcher.addPath(m_notebooksRoot);

    refreshInternal();
    emit notebooksRootChanged();
}

int NotebookManager::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_items.size();
}

QVariant NotebookManager::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size())
        return {};

    const NotebookItem &item = m_items.at(index.row());
    switch (role) {
    case NameRole:     return item.name;
    case PathRole:     return item.path;
    case ParentPathRole: return item.parentPath;
    case DepthRole:    return item.depth;
    case IsExpandedRole: return item.isExpanded;
    case IsPageRole:   return item.isPage;
    }
    return {};
}

QHash<int, QByteArray> NotebookManager::roleNames() const {
    return {
        {NameRole, "itemName"},
        {PathRole, "itemPath"},
        {ParentPathRole, "parentPath"},
        {DepthRole, "depth"},
        {IsExpandedRole, "isExpanded"},
        {IsPageRole, "isPage"},
    };
}

void NotebookManager::scanDirectory(const QString &dirPath, int depth,
                                     const QString &parentPath,
                                     const QSet<QString> &expandedPaths) {
    if (depth > 1)
        return;

    QDir dir(dirPath);
    if (!dir.exists())
        return;

    const QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    // At root level, show .md files first (above tabs)
    if (depth == 0) {
        for (const QFileInfo &info : entries) {
            if (info.isFile() && info.suffix().toLower() == "md") {
                m_items.append({
                    info.completeBaseName(),
                    info.absoluteFilePath(),
                    QString(),
                    0,
                    false,
                    true
                });
            }
        }
    }

    for (const QFileInfo &info : entries) {
        if (info.isDir() && depth == 0) {
            // Tab (directory at depth 0)
            bool expanded = expandedPaths.contains(info.absoluteFilePath());
            m_items.append({
                info.fileName(),
                info.absoluteFilePath(),
                parentPath,
                depth,
                expanded,
                false
            });
            if (expanded)
                scanDirectory(info.absoluteFilePath(), depth + 1, info.absoluteFilePath(), expandedPaths);
        } else if (info.isFile() && info.suffix().toLower() == "md" && depth == 1) {
            // Page (.md file under a tab)
            m_items.append({
                info.completeBaseName(),
                info.absoluteFilePath(),
                parentPath,
                depth,
                false,
                true
            });
        }
    }
}

void NotebookManager::refreshInternal() {
    // Save expanded states before clearing
    QSet<QString> expandedPaths;
    for (const auto &item : m_items) {
        if (item.isExpanded)
            expandedPaths.insert(item.path);
    }

    beginResetModel();
    m_items.clear();
    if (!m_notebooksRoot.isEmpty())
        scanDirectory(m_notebooksRoot, 0, QString(), expandedPaths);
    endResetModel();
    emit countChanged();
}

void NotebookManager::refresh() {
    refreshInternal();
}

QString NotebookManager::createNotebook(const QString &name) {
    if (name.trimmed().isEmpty())
        return {};

    QString safeName = name;
    safeName.replace(QRegularExpression(QStringLiteral("[/\\\\:\\x00-\\x1f\\x7f]")), QStringLiteral("-"));
    safeName = safeName.trimmed();

    const QString docsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = docsDir + QStringLiteral("/") + safeName;
    if (QDir(path).exists())
        return {};

    QDir().mkpath(path);

    // Create default title page
    QFile titlePage(path + QStringLiteral("/Untitled.md"));
    if (titlePage.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&titlePage);
        out << "# " << safeName << "\n";
        titlePage.close();
    }

    openNotebook(path);
    return path;
}

void NotebookManager::openNotebook(const QString &path) {
    if (path.isEmpty() || !QDir(path).exists())
        return;

    // Stop watching old root
    const QStringList dirs = m_watcher.directories();
    if (!dirs.isEmpty())
        m_watcher.removePaths(dirs);

    m_notebooksRoot = path;
    m_currentNotebook = path;

    if (QDir(m_notebooksRoot).exists())
        m_watcher.addPath(m_notebooksRoot);

    refreshInternal();
    saveRecent();

    emit notebooksRootChanged();
    emit currentNotebookChanged();
}

QStringList NotebookManager::recentNotebooks() const {
    QSettings settings;
    return settings.value(QStringLiteral("notebooks/recent")).toStringList();
}

void NotebookManager::removeRecent(const QString &path) {
    QSettings settings;
    QStringList recent = settings.value(QStringLiteral("notebooks/recent")).toStringList();
    recent.removeAll(path);
    settings.setValue(QStringLiteral("notebooks/recent"), recent);
}

void NotebookManager::saveRecent() {
    QSettings settings;
    QStringList recent = settings.value(QStringLiteral("notebooks/recent")).toStringList();

    // Move to front if already exists, otherwise prepend
    recent.removeAll(m_notebooksRoot);
    recent.prepend(m_notebooksRoot);

    // Keep max 10
    while (recent.size() > 10)
        recent.removeLast();

    settings.setValue(QStringLiteral("notebooks/recent"), recent);
    settings.setValue(QStringLiteral("notebooks/lastOpened"), m_notebooksRoot);
}

void NotebookManager::loadLastOpened() {
    QSettings settings;
    const QString last = settings.value(QStringLiteral("notebooks/lastOpened")).toString();
    if (!last.isEmpty() && QDir(last).exists()) {
        m_notebooksRoot = last;
        m_currentNotebook = last;
    }
}

void NotebookManager::clearLastOpened() {
    QSettings settings;
    settings.remove(QStringLiteral("notebooks/lastOpened"));
}

QString NotebookManager::createTab(const QString &name) {
    if (name.trimmed().isEmpty())
        return {};

    // Sanitize name for filesystem
    QString safeName = name;
    safeName.replace(QRegularExpression(QStringLiteral("[/\\\\:\\x00-\\x1f\\x7f]")), QStringLiteral("-"));
    safeName = safeName.trimmed();

    const QString path = m_notebooksRoot + QStringLiteral("/") + safeName;
    if (QDir(path).exists())
        return {};

    QDir().mkpath(path);
    refreshInternal();
    return path;
}

QString NotebookManager::createPage(const QString &tabPath, const QString &name) {
    if (tabPath.isEmpty())
        return {};

    QString baseName = name.trimmed();
    if (baseName.isEmpty())
        baseName = QStringLiteral("Untitled");
    baseName.replace(QRegularExpression(QStringLiteral("[/\\\\:\\x00-\\x1f\\x7f]")), QStringLiteral("-"));

    // Find a unique filename
    QString safeName = baseName;
    if (!safeName.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive))
        safeName += QStringLiteral(".md");

    QString path = tabPath + QStringLiteral("/") + safeName;
    int counter = 2;
    while (QFile::exists(path)) {
        safeName = baseName + QStringLiteral(" %1.md").arg(counter);
        path = tabPath + QStringLiteral("/") + safeName;
        counter++;
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.close();
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i].path == tabPath && !m_items[i].isExpanded) {
                m_items[i].isExpanded = true;
                break;
            }
        }
        refreshInternal();
        return path;
    }
    return {};
}

QString NotebookManager::convertFileToNotebook(const QString &filePath, const QString &name) {
    if (filePath.isEmpty() || name.trimmed().isEmpty())
        return {};

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile())
        return {};

    QString safeName = name;
    safeName.replace(QRegularExpression(QStringLiteral("[/\\\\:\\x00-\\x1f\\x7f]")), QStringLiteral("-"));
    safeName = safeName.trimmed();

    const QString docsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString notebookDir = docsDir + QStringLiteral("/") + safeName;
    if (QDir(notebookDir).exists())
        return {};

    QDir().mkpath(notebookDir);

    // Move the file into the notebook directory
    const QString destPath = notebookDir + QStringLiteral("/") + fileInfo.fileName();
    if (!QFile::rename(filePath, destPath)) {
        QDir().rmdir(notebookDir);
        return {};
    }

    openNotebook(notebookDir);
    return notebookDir;
}

QString NotebookManager::convertContentToNotebook(const QString &content, const QString &name) {
    if (name.trimmed().isEmpty())
        return {};

    QString safeName = name;
    safeName.replace(QRegularExpression(QStringLiteral("[/\\\\:\\x00-\\x1f\\x7f]")), QStringLiteral("-"));
    safeName = safeName.trimmed();

    const QString docsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString notebookDir = docsDir + QStringLiteral("/") + safeName;

    if (!QDir(notebookDir).exists())
        QDir().mkpath(notebookDir);

    // Write content to a new file in the notebook
    QString fileName = QStringLiteral("Untitled.md");
    QString filePath = notebookDir + QStringLiteral("/") + fileName;
    int counter = 2;
    while (QFile::exists(filePath)) {
        fileName = QStringLiteral("Untitled %1.md").arg(counter);
        filePath = notebookDir + QStringLiteral("/") + fileName;
        counter++;
    }

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << content;
        file.close();
    }

    openNotebook(notebookDir);
    return filePath;
}

void NotebookManager::renameItem(const QString &path, const QString &newName) {
    if (path.isEmpty() || newName.trimmed().isEmpty())
        return;

    QFileInfo info(path);
    if (!info.exists())
        return;

    QString safeName = newName;
    safeName.replace(QRegularExpression(QStringLiteral("[/\\\\:\\x00-\\x1f\\x7f]")), QStringLiteral("-"));
    safeName = safeName.trimmed();

    if (info.isFile() && !safeName.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive))
        safeName += QStringLiteral(".md");

    const QString newPath = info.absolutePath() + QStringLiteral("/") + safeName;
    if (QFile::exists(newPath) || QDir(newPath).exists())
        return;

    if (info.isDir())
        QDir().rename(path, newPath);
    else
        QFile::rename(path, newPath);

    refreshInternal();
}

void NotebookManager::deleteItem(const QString &path) {
    if (path.isEmpty())
        return;

    QFileInfo info(path);
    if (!info.exists())
        return;

    // Don't allow deleting the root
    if (info.absoluteFilePath() == m_notebooksRoot)
        return;

    if (info.isDir()) {
        QDir dir(path);
        dir.removeRecursively();
    } else {
        QFile::remove(path);
    }

    refreshInternal();
}

void NotebookManager::toggleExpanded(const QString &path) {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].path == path && !m_items[i].isPage) {
            m_items[i].isExpanded = !m_items[i].isExpanded;
            refreshInternal();
            return;
        }
    }
}

void NotebookManager::movePageToTab(const QString &pagePath, const QString &destTabPath) {
    if (pagePath.isEmpty() || destTabPath.isEmpty())
        return;

    QFileInfo fileInfo(pagePath);
    if (!fileInfo.exists() || !fileInfo.isFile())
        return;

    QDir destDir(destTabPath);
    if (!destDir.exists())
        return;

    const QString destPath = destTabPath + QStringLiteral("/") + fileInfo.fileName();
    if (QFile::exists(destPath))
        return;

    if (QFile::rename(pagePath, destPath))
        refreshInternal();
}

void NotebookManager::deleteNotebook(const QString &path) {
    if (path.isEmpty())
        return;

    QDir dir(path);
    if (!dir.exists())
        return;

    // Stop watching
    const QStringList dirs = m_watcher.directories();
    if (!dirs.isEmpty())
        m_watcher.removePaths(dirs);

    dir.removeRecursively();
    removeRecent(path);

    if (m_notebooksRoot == path) {
        m_notebooksRoot.clear();
        m_currentNotebook.clear();
        m_items.clear();
        beginResetModel();
        endResetModel();
        emit countChanged();
        emit notebooksRootChanged();
        emit currentNotebookChanged();
    }
}

QVariantMap NotebookManager::itemAt(int index) const {
    if (index < 0 || index >= m_items.size())
        return {};

    const NotebookItem &item = m_items.at(index);
    return {
        {"name", item.name},
        {"path", item.path},
        {"parentPath", item.parentPath},
        {"depth", item.depth},
        {"isExpanded", item.isExpanded},
        {"isPage", item.isPage},
    };
}

int NotebookManager::findItemByPath(const QString &path) const {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].path == path)
            return i;
    }
    return -1;
}
