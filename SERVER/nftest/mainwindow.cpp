#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <stdlib.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    pro=new QProcess;
    QObject::connect(pro,SIGNAL(readyRead()),this,SLOT(readOutput()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    system("sudo iptables -F");
    ui->textEdit->append("run iptables -F to clear all queues!");
}

void MainWindow::on_pushButton_2_clicked()
{
    pro->start("/home/goldfish22/Desktop/tcp");
    //pro->start("ifconfig");
}

void MainWindow::readOutput()
{
    out=pro->readAll();
    ui->textEdit->append(out);
}
void MainWindow::on_pushButton_3_clicked()
{
    pro->start("/home/goldfish22/Desktop/udp");
    //pro->start("ls -al");
}
