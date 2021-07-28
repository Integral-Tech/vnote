#include "historymgr.h"

#include <QDebug>

#include "configmgr.h"
#include "sessionconfig.h"
#include "coreconfig.h"
#include "vnotex.h"
#include "notebookmgr.h"
#include <notebook/notebook.h>
#include <notebookbackend/inotebookbackend.h>

using namespace vnotex;

bool HistoryItemFull::operator<(const HistoryItemFull &p_other) const
{
    if (m_item.m_lastAccessedTimeUtc < p_other.m_item.m_lastAccessedTimeUtc) {
        return true;
    } else if (m_item.m_lastAccessedTimeUtc > p_other.m_item.m_lastAccessedTimeUtc) {
        return false;
    } else {
        return m_item.m_path < p_other.m_item.m_path;
    }
}


int HistoryMgr::s_maxHistoryCount = 100;

HistoryMgr::HistoryMgr()
{
    s_maxHistoryCount = ConfigMgr::getInst().getCoreConfig().getHistoryMaxCount();

    connect(&VNoteX::getInst().getNotebookMgr(), &NotebookMgr::notebooksUpdated,
            this, &HistoryMgr::loadHistory);

    loadHistory();
}

static bool historyPtrCmp(const QSharedPointer<HistoryItemFull> &p_a, const QSharedPointer<HistoryItemFull> &p_b)
{
    return *p_a < *p_b;
}

void HistoryMgr::loadHistory()
{
    m_history.clear();

    // Load from session.
    {
        const auto &history = ConfigMgr::getInst().getSessionConfig().getHistory();
        for (const auto &item : history) {
            auto fullItem = QSharedPointer<HistoryItemFull>::create();
            fullItem->m_item = item;
            m_history.push_back(fullItem);
        }
    }

    // Load from notebooks.
    {
        const auto &notebooks = VNoteX::getInst().getNotebookMgr().getNotebooks();
        for (const auto &nb : notebooks) {
            const auto &history = nb->getHistory();
            const auto &backend = nb->getBackend();
            for (const auto &item : history) {
                auto fullItem = QSharedPointer<HistoryItemFull>::create();
                fullItem->m_item = item;
                fullItem->m_item.m_path = backend->getFullPath(item.m_path);
                fullItem->m_notebookName = nb->getName();
                m_history.push_back(fullItem);
            }
        }
    }

    std::sort(m_history.begin(), m_history.end(), historyPtrCmp);

    qDebug() << "loaded" << m_history.size() << "history items";

    emit historyUpdated();
}

const QVector<QSharedPointer<HistoryItemFull>> &HistoryMgr::getHistory() const
{
    return m_history;
}

void HistoryMgr::add(const QString &p_path,
                     int p_lineNumber,
                     ViewWindowMode p_mode,
                     bool p_readOnly,
                     Notebook *p_notebook)
{
    if (p_path.isEmpty() || s_maxHistoryCount == 0) {
        return;
    }

    HistoryItem item(p_path, p_lineNumber, QDateTime::currentDateTimeUtc());

    if (p_notebook) {
        p_notebook->addHistory(item);
    } else {
        auto &sessionConfig = ConfigMgr::getInst().getSessionConfig();
        sessionConfig.addHistory(item);
    }

    // Maintain the combined queue.
    {
        for (int i = m_history.size() - 1; i >= 0; --i) {
            if (m_history[i]->m_item.m_path == item.m_path) {
                // Erase it.
                m_history.remove(i);
                break;
            }
        }

        auto fullItem = QSharedPointer<HistoryItemFull>::create();
        fullItem->m_item = item;
        if (p_notebook) {
            fullItem->m_notebookName = p_notebook->getName();
        }
        m_history.append(fullItem);
    }

    // Update m_lastClosedFiles.
    {
        for (int i = m_lastClosedFiles.size() - 1; i >= 0; --i) {
            if (m_lastClosedFiles[i].m_path == p_path) {
                m_lastClosedFiles.remove(i);
                break;
            }
        }

        m_lastClosedFiles.append(LastClosedFile());
        auto &file = m_lastClosedFiles.back();
        file.m_path = p_path;
        file.m_lineNumber = p_lineNumber;
        file.m_mode = p_mode;
        file.m_readOnly = p_readOnly;

        if (m_lastClosedFiles.size() > 100) {
            m_lastClosedFiles.remove(0, m_lastClosedFiles.size() - 100);
        }
    }

    emit historyUpdated();
}

void HistoryMgr::insertHistoryItem(QVector<HistoryItem> &p_history, const HistoryItem &p_item)
{
    for (int i = p_history.size() - 1; i >= 0; --i) {
        if (p_history[i].m_path == p_item.m_path) {
            // Erase it.
            p_history.remove(i);
            break;
        }
    }

    p_history.append(p_item);

    if (p_history.size() > s_maxHistoryCount) {
        p_history.remove(0, p_history.size() - s_maxHistoryCount);
    }
}

void HistoryMgr::clear()
{
    ConfigMgr::getInst().getSessionConfig().clearHistory();

    const auto &notebooks = VNoteX::getInst().getNotebookMgr().getNotebooks();
    for (const auto &nb : notebooks) {
        nb->clearHistory();
    }

    loadHistory();
}

HistoryMgr::LastClosedFile HistoryMgr::popLastClosedFile()
{
    if (m_lastClosedFiles.isEmpty()) {
        return LastClosedFile();
    }

    auto file = m_lastClosedFiles.back();
    m_lastClosedFiles.pop_back();
    return file;
}
