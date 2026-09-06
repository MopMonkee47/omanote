#pragma once

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

struct NotebookItem {
    QString name;
    QString path;         // absolute filesystem path
    QString parentPath;   // parent directory (empty for tabs)
    int depth;            // 0=tab, 1=page
    bool isExpanded;      // only meaningful for depth 0
    bool isPage;          // true for .md files
};

class NotebookManager : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString notebooksRoot READ notebooksRoot WRITE setNotebooksRoot NOTIFY notebooksRootChanged)
    Q_PROPERTY(QString currentNotebook READ currentNotebook NOTIFY currentNotebookChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        ParentPathRole,
        DepthRole,
        IsExpandedRole,
        IsPageRole,
    };

    explicit NotebookManager(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = NameRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString notebooksRoot() const { return m_notebooksRoot; }
    void setNotebooksRoot(const QString &root);
    QString currentNotebook() const { return m_currentNotebook; }
    int count() const { return m_items.size(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString createNotebook(const QString &name);
    Q_INVOKABLE void openNotebook(const QString &path);
    Q_INVOKABLE QStringList recentNotebooks() const;
    Q_INVOKABLE void removeRecent(const QString &path);
    Q_INVOKABLE QString createTab(const QString &name);
    Q_INVOKABLE QString createPage(const QString &tabPath, const QString &name);
    Q_INVOKABLE QString convertFileToNotebook(const QString &filePath, const QString &name);
    Q_INVOKABLE QString convertContentToNotebook(const QString &content, const QString &name);
    Q_INVOKABLE void renameItem(const QString &path, const QString &newName);
    Q_INVOKABLE void deleteItem(const QString &path);
    Q_INVOKABLE void toggleExpanded(const QString &path);
    Q_INVOKABLE void movePageToTab(const QString &pagePath, const QString &destTabPath);
    Q_INVOKABLE void deleteNotebook(const QString &path);
    Q_INVOKABLE QVariantMap itemAt(int index) const;
    Q_INVOKABLE int findItemByPath(const QString &path) const;
    Q_INVOKABLE void clearLastOpened();

signals:
    void notebooksRootChanged();
    void currentNotebookChanged();
    void countChanged();

private:
    void scanDirectory(const QString &dirPath, int depth, const QString &parentPath,
                       const QSet<QString> &expandedPaths);
    void refreshInternal();
    void saveRecent();
    void loadLastOpened();

    QString m_notebooksRoot;
    QString m_currentNotebook;
    QVector<NotebookItem> m_items;
    QFileSystemWatcher m_watcher;
};
