#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QTime>
#include <QTimer>
#include <QEvent>
#include <fstream>
#include "MainWindow.h"
#include "ClockWindow.h"
#include "ui_MainWindow.h"
#include <QDebug>


bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::StatusTip) return true;
    return false;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    qApp->installEventFilter(this);
    ui->toolBar->setCursor(Qt::PointingHandCursor);

    clocks = {};
    isClosestExists = false;

    QStringList horizHeaders;
    horizHeaders << "№" << "Name" << "Type" << "Status" << "Value" << "End time" << "Time left" << "Repeating";
    ui->table->setColumnCount(horizHeaders.size());
    ui->table->setHorizontalHeaderLabels(horizHeaders);
    ui->table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->table->verticalHeader()->setDefaultSectionSize(20);

    std::ifstream out;

    out.open("config.dat");
    if(out.is_open())
    {
        size_t width;
        int sortingCol;
        for(size_t i = 0; i<ui->table->horizontalHeader()->count(); i++) //read columns width
        {
            out >> width;
            ui->table->setColumnWidth(i, width);
        }
        out >> sortingCol; //read the sorting column number (or -1, if no sorting)
        if(sortingCol != -1)
        {
            int sortingOrder;
            out >> sortingOrder;
            if(sortingOrder == 1) ui->table->horizontalHeader()->setSortIndicator(sortingCol, Qt::SortOrder::DescendingOrder);
            else ui->table->horizontalHeader()->setSortIndicator(sortingCol, Qt::SortOrder::AscendingOrder);
        }
        out.close();
    }

    out.open("clocks.dat");
    if(out.is_open())
    {
        Clock clock(this);
        ui->table->setSortingEnabled(false);
        while(out >> clock)
        {
            clocks.push_back(clock);
            ui->table->insertRow(clock.getId());
            for(size_t i = 0; i<ui->table->horizontalHeader()->count(); i++)
            {
                ui->table->setItem(clock.getId(), i, new QTableWidgetItem());
            }
            clock.printToTable(clock.getId());
        }
        ui->table->setSortingEnabled(true);
        out.close();
    }

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(counting()));
    timer->start(500);
}

MainWindow::~MainWindow()
{
    std::ofstream in;

    in.open("config.dat", std::ios_base::trunc);
    if(in.is_open())
    {
        for(size_t i = 0; i<ui->table->horizontalHeader()->count(); i++) //write columns width
        {
            in << ui->table->horizontalHeader()->sectionSize(i) << " ";
        }
        if(ui->table->horizontalHeader()->sortIndicatorSection() != ui->table->horizontalHeader()->count()) //is sorted?
        {
            in << ui->table->horizontalHeader()->sortIndicatorSection() << " "; //write the sorting column number
            in << ui->table->horizontalHeader()->sortIndicatorOrder(); //write the sorting order
        }
        else in << -1;
        in.close();
    }

    in.open("clocks.dat", std::ios_base::trunc);
    if(in.is_open())
    {
        for(size_t i = 0; i<clocks.size(); i++) //write clocks data
        {
            in << clocks[i];
            if(i != clocks.size()-1) in << "\n";
        }
        in.close();
    }
    delete ui;
}

void MainWindow::counting()
{
    this->setWindowTitle("SmartClock     " + QTime::currentTime().toString("hh:mm:ss"));
    QTime timeLeft;
    size_t index, secLeft;
    ui->table->setSortingEnabled(false);
    for(size_t i = 0; i<clocks.size(); i++)
    {
        index = (ui->table->item(i,0)->data(Qt::DisplayRole)).toULongLong()-1;
        if(clocks[index].getStatus()!=0)
        {
            secLeft = clocks[index].getEndTime() - QDateTime::currentSecsSinceEpoch();
            timeLeft = QTime(0,0,0).addSecs(secLeft);
            ui->table->item(i,6)->setText(timeLeft.toString("hh : mm : ss"));
            if(secLeft<=0)
            {
                //WINDOW

                isClosestExists = false;
            }
            if(!isClosestExists)
            {
                indexOfClosest = index;
                isClosestExists = true;
            }
            else if(clocks[index].getEndTime()<clocks[indexOfClosest].getEndTime())
            {
                indexOfClosest = index;
            }
        }
    }
    ui->table->setSortingEnabled(true);
    if(isClosestExists)
    {
        timeLeft = QTime(0,0,0).addSecs(clocks[indexOfClosest].getEndTime() - QDateTime::currentSecsSinceEpoch());
        ui->statusbar->showMessage(timeLeft.toString("hh : mm : ss - ")+clocks[indexOfClosest].getName());
    }
    else
    {
        ui->statusbar->showMessage("No active clocks");
    }
}

void MainWindow::addNewClockWindow()
{
    auto clockWindow = new ClockWindow(this);
    clockWindow->setModal(true);
    clockWindow->setWindowTitle("New Clock");
    clockWindow->show();
}

void MainWindow::editClockWindow()
{
    QModelIndexList selected = ui->table->selectionModel()->selectedRows();
    size_t index = (ui->table->item(selected[0].row(),0)->data(Qt::DisplayRole)).toULongLong()-1;
    if(clocks[index].getStatus()==1)
    {
        QMessageBox msgBox(QMessageBox::Critical, "Error", "You can't edit the active clock.");
        msgBox.setStyleSheet("QMessageBox QPushButton{"
                                "background-color: rgb(220, 240, 255);}"
                             "QMessageBox{"
                                "background-color: rgb(129, 190, 255);}");
        msgBox.exec();
        return;
    }
    auto clockWindow = new ClockWindow(this, &clocks[index]);
    clockWindow->setModal(true);
    clockWindow->setWindowTitle("Edit Clock");
    clockWindow->show();
}

void MainWindow::addNewClock(Clock *clock)
{
    clock->setStatus(0);
    clock->setId(this->clocks.size());
    clock->setEndTime(0);
    this->clocks.push_back(*clock);
    size_t row = ui->table->rowCount();
    ui->table->setSortingEnabled(false);
    ui->table->insertRow(row);
    for(size_t i = 0; i<ui->table->horizontalHeader()->count(); i++)
    {
        ui->table->setItem(row, i, new QTableWidgetItem());
    }
    clock->printToTable(row);
    ui->table->setSortingEnabled(true);
    delete clock;
}

void MainWindow::editClock(Clock* clock)
{
    QModelIndexList selected = ui->table->selectionModel()->selectedRows();
    size_t index = (ui->table->item(selected[0].row(),0)->data(Qt::DisplayRole)).toULongLong()-1;
    clock->setStatus(0);
    clock->setId(index);
    clocks[index] = *clock;
    ui->table->setSortingEnabled(false);
    clock->printToTable(selected[0].row());
    ui->table->setSortingEnabled(true);
    delete clock;
}

void MainWindow::removeClocks()
{
    bool removingClosest = false;
    size_t index;
    QString string = "";
    std::vector<size_t> removed;
    QModelIndexList selected = ui->table->selectionModel()->selectedRows();
    for(size_t i = 0; i<selected.size(); i++)
    {
        index = (ui->table->item(selected[i].row(),0)->data(Qt::DisplayRole)).toULongLong()-1;
        auto pos = std::upper_bound(removed.begin(), removed.end(), index);
        removed.insert(pos, index);
        if(clocks[index].getStatus()!=0)
        {
            string += clocks[index].getName()+"\n";
        }
        if(index == indexOfClosest)
        {
            removingClosest = true;
        }
    }
    if(string!="")
    {
        QMessageBox msgBox(QMessageBox::Question, "", "This action will remove the following active clocks:\n\n"+string+"\nAre you sure?", QMessageBox::Yes|QMessageBox::No);
        msgBox.setStyleSheet("QMessageBox QPushButton{"
                                "background-color: rgb(220, 240, 255);}"
                             "QMessageBox{"
                                "background-color: rgb(129, 190, 255);}");
        auto button = msgBox.exec();
        if(button == QMessageBox::No)
        {
            return;
        }
    }

    if(removingClosest) isClosestExists = false;
    size_t currIndex;
    ui->table->setSortingEnabled(false);
    while(!selected.empty()) //removing rows from the table, recovering numeration in the table
    {
        index = (ui->table->item(selected[0].row(),0)->data(Qt::DisplayRole)).toULongLong()-1;
        ui->table->removeRow(selected[0].row());
        for(size_t i = 0; i<ui->table->rowCount(); i++)
        {
            currIndex = (ui->table->item(i,0)->data(Qt::DisplayRole)).toULongLong()-1;
            if(currIndex > index)
            {
                ui->table->item(i,0)->setData(Qt::DisplayRole, currIndex);
            }
        }
        selected = ui->table->selectionModel()->selectedRows();
    }
    ui->table->setSortingEnabled(true);
    for(size_t i = removed.size()-1; i<removed.size(); i--) //removing clocks from the vector
    {
        clocks.erase(clocks.begin()+removed[i]);
    }
    for(size_t i = clocks.size()-1; i<clocks.size(); i--) //recovering numeration in the vector
    {
        clocks[i].setId(i);
    }
}
void MainWindow::startClocks()
{
    size_t index;
    QString string = "";
    QModelIndexList selected = ui->table->selectionModel()->selectedRows();
    for(size_t i = 0; i<selected.size(); i++)
    {
        index = (ui->table->item(selected[i].row(),0)->data(Qt::DisplayRole)).toULongLong()-1;
        if(clocks[index].getStatus()!=0)
        {
            string += clocks[index].getName()+"\n";
        }
    }
    if(string!="")
    {
        QMessageBox msgBox(QMessageBox::Question, "", "The following clocks are already active:\n\n"+string+"\nRestart them?", QMessageBox::Yes|QMessageBox::No);
        msgBox.setStyleSheet("QMessageBox QPushButton{"
                                "background-color: rgb(220, 240, 255);}"
                             "QMessageBox{"
                                "background-color: rgb(129, 190, 255);}");
        auto button = msgBox.exec();
        if(button==QMessageBox::No)
        {
             for(size_t i = 0; i<selected.size(); i++)
             {
                  index = (ui->table->item(selected[i].row(),0)->data(Qt::DisplayRole)).toULongLong()-1;
                  if(clocks[index].getStatus()!=0)
                  {
                      selected.erase(selected.begin()+i);
                      i--;
                  }
             }
        }
    }
    ui->table->setSortingEnabled(false);
    for(size_t i = 0; i<selected.size(); i++)
    {
        index = (ui->table->item(selected[i].row(),0)->data(Qt::DisplayRole)).toULongLong()-1;
        clocks[index].setStatus(1);
        if(clocks[index].getType()==0) //timer
        {
            qint64 valueToSec = clocks[index].getValue().hour()*3600 + clocks[index].getValue().minute()*60 + clocks[index].getValue().second();
            clocks[index].setEndTime(QDateTime::currentSecsSinceEpoch() + valueToSec);
        }
        else if(clocks[index].getType()==1) //alarm clock
        {
            qint64 valueToSec = clocks[index].getValue().hour()*3600 + clocks[index].getValue().minute()*60 + clocks[index].getValue().second();
            qint64 currentTimeToSec = QTime::currentTime().hour()*3600 + QTime::currentTime().minute()*60 + QTime::currentTime().second();
            if(QTime::currentTime()<=clocks[index].getValue())
            {
                clocks[index].setEndTime(QDateTime::currentSecsSinceEpoch() - currentTimeToSec + valueToSec);
            }
            else
            {
                const qint64 secInDay = 24*3600;
                clocks[index].setEndTime(QDateTime::currentSecsSinceEpoch() - currentTimeToSec + valueToSec + secInDay);
            }
        }
        clocks[index].printToTable(selected[i].row());
    }
    ui->table->setSortingEnabled(true);
}
void MainWindow::stopClocks()
{
    size_t index;
    QModelIndexList selected = ui->table->selectionModel()->selectedRows();
    ui->table->setSortingEnabled(false);
    for(size_t i = 0; i<selected.size(); i++)
    {
        index = (ui->table->item(selected[i].row(),0)->data(Qt::DisplayRole)).toULongLong()-1;
        clocks[index].setStatus(0);
        clocks[index].setEndTime(0);
        clocks[index].printToTable(selected[i].row());
        if(index == indexOfClosest)
        {
            isClosestExists = false;
        }
    }
    ui->table->setSortingEnabled(true);
}

void MainWindow::on_table_customContextMenuRequested(const QPoint &pos)
{
    QMenu *menu = new QMenu(this);
    menu->setStyleSheet("QMenu{background-color: rgb(204, 232, 255);}"
                        "QMenu Action:hover{background-color: red;}");
    QAction *add_new(nullptr), *start(nullptr), *stop(nullptr), *edit(nullptr), *remove(nullptr);

    QModelIndexList selected = ui->table->selectionModel()->selectedRows();
    if(selected.size()==0)
    {
        add_new = new QAction("Add new", this);
        menu->addAction(add_new);

    }
    else if(selected.size()==1)
    {
        start = new QAction("Start", this);
        stop = new QAction("Stop", this);
        edit = new QAction("Edit", this);
        remove = new QAction("Remove", this);
        menu->addAction(start);
        menu->addAction(stop);
        menu->addSeparator();
        menu->addAction(edit);
        menu->addAction(remove);
    }
    else
    {
        start = new QAction("Start", this);
        stop = new QAction("Stop", this);
        remove = new QAction("Remove", this);
        menu->addAction(start);
        menu->addAction(stop);
        menu->addSeparator();
        menu->addAction(remove);
    }

    if(add_new) connect(add_new, SIGNAL(triggered()), this, SLOT(addNewClockWindow()));
    if(edit) connect(edit, SIGNAL(triggered()), this, SLOT(editClockWindow()));
    if(remove) connect(remove, SIGNAL(triggered()), this, SLOT(removeClocks()));
    if(start) connect(start, SIGNAL(triggered()), this, SLOT(startClocks()));
    if(stop) connect(stop, SIGNAL(triggered()), this, SLOT(stopClocks()));

    menu->popup(ui->table->viewport()->mapToGlobal(pos));
}
