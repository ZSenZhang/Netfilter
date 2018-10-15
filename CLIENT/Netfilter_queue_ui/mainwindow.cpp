#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <stdlib.h>
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->pro=new QProcess;
    QObject::connect(pro,SIGNAL(readyRead()),this,SLOT(readOutput()));
     ui->comboBox->addItem("");
        ui->comboBox->addItem("RSA");
        ui->comboBox->addItem("DH");
       // ui->comboBox_2->addItem("filter");
            ui->comboBox_2->addItem("Encryption");
            ui->comboBox_2->addItem("Filter");
            //ui->comboBox_3->addItem("all");
             ui->comboBox_3->addItem("Default");
            ui->comboBox_3->addItem("Ping");
            ui->comboBox_3->addItem("TCP");
            ui->comboBox_3->addItem("UDP");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    QString cmd;
        cmd.append("/home/goldfish22/Desktop/queue_fw");

        if(ui->comboBox->currentText()!="")
        {
            cmd.append(" -e ");
            cmd.append(ui->comboBox->currentText());
        }
        if(ui->comboBox_3->currentText()!="Default")
        {
            cmd.append(" -p ");
            cmd.append(ui->comboBox_3->currentText());
        }
        if(ui->lineEdit->text()!="")
        {
            cmd.append(" -t ");
            cmd.append(ui->lineEdit->text());
            cmd.append(" -f ");
            cmd.append(ui->lineEdit->text());
        }
        if(ui->lineEdit_2->text()!="")
        {
            cmd.append(" -x ");
            cmd.append(ui->lineEdit_2->text());
        }
        if(ui->lineEdit_3->text()!="")
        {
            cmd.append(" -m ");
            cmd.append(ui->lineEdit_3->text());
        }
        if(ui->lineEdit_4->text()!="")
        {
            cmd.append(" -y ");
            cmd.append(ui->lineEdit_4->text());
        }
        if(ui->lineEdit_5->text()!="")
        {
            cmd.append(" -n ");
            cmd.append(ui->lineEdit_5->text());
        }
        //ui->textEdit->setText(cmd);
        pro->start(cmd);
        out=tr("");
        ui->textEdit->setText(out);
}

void MainWindow::readOutput()
{
    out=pro->readAll();
    ui->textEdit->append(out);
}

void MainWindow::on_pushButton_2_clicked()
{
    pro->close();
    system("sudo iptables -F");
}
