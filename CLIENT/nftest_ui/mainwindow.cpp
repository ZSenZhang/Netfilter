#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <stdlib.h>
#include <cstring>
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    system("sudo iptables -F");
    ui->textEdit->append("run iptables -F!");
}

void MainWindow::on_pushButton_2_clicked()
{
    QString s,cmd;
    std::string cmd1;
    const char* cmd2;
    s=ui->lineEdit->text();
    cmd="/home/goldfish22/Desktop/tcp "+s;
    cmd1=cmd.toStdString();
    cmd2=cmd1.c_str();
    system(cmd2);
    ui->textEdit->append("send a tcp packet!");
}

void MainWindow::on_pushButton_3_clicked()
{
    system("/home/goldfish22/Desktop/udp");
    ui->textEdit->append("send a udp packet!");
}
