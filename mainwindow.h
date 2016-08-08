#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsView>
#include "canvas.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
private:
    Canvas *canvas;
    UndoRedoStack mUndoRedoStack;
protected:
    void keyPressEvent(QKeyEvent *event);
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void addUndoRedo(UndoRedo undoRedo);
};

#endif // MAINWINDOW_H
