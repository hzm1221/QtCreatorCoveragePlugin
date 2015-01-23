#include "Visualizer.h"

#include "Mark.h"
#include "MarkManager.h"
#include "Node.h"
#include "FileNode.h"
#include "ProjectTreeManager.h"
#include "LinePainter.h"
#include "LineCleaner.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <texteditor/texteditor.h>

#include <QPlainTextEdit>
#include <QScrollBar>

#include <QDebug>

Visualizer::Visualizer(ProjectTreeManager *projectTreeManager, QAction *renderAction, QObject *parent) :
    QObject(parent),
    projectTreeManager(projectTreeManager),
    markManager(new MarkManager(this)),
    renderAction(renderAction)
{
    using namespace Core;
    EditorManager *editorManager = EditorManager::instance();

    connect(editorManager,SIGNAL(currentEditorChanged(Core::IEditor*)),SLOT(renderCoverage()),Qt::QueuedConnection);
    connect(editorManager,SIGNAL(currentEditorChanged(Core::IEditor*)),SLOT(bindEditorToPainting(Core::IEditor*)));

    connect(renderAction,SIGNAL(triggered(bool)),SLOT(renderCoverage()));
    connect(renderAction,SIGNAL(triggered(bool)),SLOT(repaintMarks(bool)));
}

void Visualizer::refreshMarks()
{
    markManager->removeAllMarks();

    Node *rootNode = projectTreeManager->getRootNode();
    QList<Node *> leafs = rootNode->getLeafs();

    foreach (Node *leaf, leafs) {
        FileNode *fileNode = static_cast<FileNode *>(leaf);
        QString fileName = fileNode->getFullAbsoluteName().replace(QRegExp(QLatin1String("(Sources|Headers)/")), QLatin1String(""));
        const LineHitList &lineHitList = fileNode->getLineHitList();

        foreach (const LineHit &lineHit, lineHitList) {
            markManager->addMark(fileName, lineHit.pos, lineHit.hit);
        }
    }
}

void Visualizer::renderCoverage()
{
    using namespace Core;

    EditorManager *editorManager = EditorManager::instance();
    IEditor *currentEditor = editorManager->currentEditor();
    if (!currentEditor)
        return;

    TextEditor::TextEditorWidget *textEdit = qobject_cast<TextEditor::TextEditorWidget *>(currentEditor->widget());
    if (!textEdit)
        return;

    if (renderAction->isChecked()) {
        renderCoverage(textEdit);
    } else {
        clearCoverage(textEdit);
    }
}

void Visualizer::renderCoverage(TextEditor::TextEditorWidget *textEdit) const
{
    LinePainter painter(textEdit, getLineCoverage());
    painter.render();
}

void Visualizer::clearCoverage(TextEditor::TextEditorWidget *textEdit) const
{
    LineCleaner cleaner(textEdit);
    cleaner.render();
}

void Visualizer::bindEditorToPainting(Core::IEditor *editor)
{
    if (!editor)
        return;

    if (QPlainTextEdit *plainTextEdit = qobject_cast<QPlainTextEdit *>(editor->widget())) {
        connect(plainTextEdit,SIGNAL(cursorPositionChanged()),SLOT(renderCoverage()), Qt::UniqueConnection);
        connect(plainTextEdit->verticalScrollBar(),SIGNAL(valueChanged(int)),SLOT(renderCoverage()),Qt::UniqueConnection);
    }
}

void Visualizer::repaintMarks(bool isRender)
{
    if (!isRender) {
        markManager->removeAllMarks();
    } else {
        refreshMarks();
    }
}

TextEditor::BaseTextEditor *Visualizer::currentTextEditor() const
{
    Core::EditorManager *em = Core::EditorManager::instance();
    Core::IEditor *currEditor = em->currentEditor();
    if (!currEditor)
        return 0;
    return qobject_cast<TextEditor::BaseTextEditor *>(currEditor);
}

QMap<int, int> Visualizer::getLineCoverage() const
{
    using namespace TextEditor;
    QMap<int, int> lineCoverage;

    if (BaseTextEditor *textEditor = currentTextEditor())
        if (TextDocument *textDocument = textEditor->textDocument()) {
            TextMarks marks = textDocument->marks();
            foreach (TextMark *mark, marks) {
                if (Mark *trueMark = dynamic_cast<Mark *>(mark))
                    lineCoverage.insert(trueMark->lineNumber(), trueMark->getType());
            }
        }

    return lineCoverage;
}
