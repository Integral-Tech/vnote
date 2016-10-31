#ifndef VEDIT_H
#define VEDIT_H

#include <QTextEdit>
#include <QString>
#include "vconstants.h"
#include "vnotefile.h"

class HGMarkdownHighlighter;
class VEditOperations;

class VEdit : public QTextEdit
{
    Q_OBJECT
public:
    VEdit(VNoteFile *noteFile, QWidget *parent = 0);
    ~VEdit();
    void beginEdit();

    // Save buffer content to noteFile->content.
    void saveFile();

    inline void setModified(bool modified);
    inline bool isModified() const;

    void reloadFile();

protected:
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    bool canInsertFromMimeData(const QMimeData *source) const Q_DECL_OVERRIDE;
    void insertFromMimeData(const QMimeData *source) Q_DECL_OVERRIDE;

private:
    void updateTabSettings();
    void updateFontAndPalette();

    bool isExpandTab;
    QString tabSpaces;
    VNoteFile *noteFile;
    HGMarkdownHighlighter *mdHighlighter;
    VEditOperations *editOps;
};


inline bool VEdit::isModified() const
{
    return document()->isModified();
}

inline void VEdit::setModified(bool modified)
{
    document()->setModified(modified);
}

#endif // VEDIT_H
