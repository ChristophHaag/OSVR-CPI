/*
 * Copyright 2016 OSVR and contributors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdio.h>
#include <string>
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <iostream>
#include <fstream>

#include <QFile>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QList>
#include <QMessageBox>
#include <QProcess>
#include <QDir>
#include <QtSerialPort>

#include <osvr/USBSerial/USBSerialEnum.h>
#include <osvr/USBSerial/USBSerialDevice.h>
#include <ios>
#include <iterator>

#include "json/json.h"

#define NOTFOUNDSTR "Not found"

// Helper function for validating numerical input
class myValidator : public QDoubleValidator
{
    public:
    myValidator(double bottom, double top, int decimals, QObject * parent) :
    QDoubleValidator(bottom, top, decimals, parent)
    {
    }

    QValidator::State validate(QString &s, int &i) const
    {
        if (s.isEmpty() || s == "-") {
            return QValidator::Intermediate;
        }

        QLocale locale;

        QChar decimalPoint = locale.decimalPoint();
        int charsAfterPoint = s.length() - s.indexOf(decimalPoint) -1;

        if (charsAfterPoint > decimals() && s.indexOf(decimalPoint) != -1) {
            return QValidator::Invalid;
        }

        bool ok;
        double d = locale.toDouble(s, &ok);

        if (ok && d >= bottom() && d <= top()) {
            return QValidator::Acceptable;
        } else {
            return QValidator::Invalid;
        }
    }
};

MainWindow::MainWindow(bool verbose, QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    m_verbose = verbose;
    ui->setupUi(this);

    ui->standingHeight->setValidator( new myValidator(0, 300, 2, this) );
    ui->seatedHeight->setValidator( new myValidator(0, 300, 2, this) );
    ui->ipd->setValidator(new myValidator(0,100,2,0));

    ui->dOsSpherical->setValidator( new myValidator(-100, 100, 2, this) );
    ui->dOsCylindrical->setValidator( new myValidator(-100, 100, 2, this) );
    ui->dOsAxis->setValidator( new myValidator(-100, 300, 2, this) );
    ui->nOsAdd->setValidator( new myValidator(-100, 100, 2, this) );

    ui->dOdSpherical->setValidator( new myValidator(-100, 100, 2, this) );
    ui->dOdCylindrical->setValidator( new myValidator(-100, 100, 2, this) );
    ui->dOdAxis->setValidator( new myValidator(-100, 300, 2, this) );
    ui->nOdAdd->setValidator( new myValidator(-100, 100, 2, this) );

    ui->tabWidget->setCurrentIndex(0);

    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString programPath = "";
#ifdef _WIN32
    if (m_verbose) {
        qInfo() << "Running on Windows";
    }
    programPath = env.value("PROGRAMDATA","C:/ProgramData");
    m_osvrUserConfigFilename = QString(programPath+"/OSVR/osvr_user_settings.json");
#else // TODO: Mac OS X
    if (m_verbose) {
        qInfo() << "Running on Linux";
    }

    ui->enableButton->setEnabled(false);
    ui->disableButton->setEnabled(false);
    ui->updateFWButton->setEnabled(false); // TODO: implement

    QString username = env.value("USER");
    programPath = env.value("XDG_CONFIG_HOME","/home/"+username+"/.config");
    m_osvrUserConfigFilename = QString(programPath+"/osvr_user_settings.json");
#endif
    if (m_verbose) {
        qInfo() << "Using config file: " << m_osvrUserConfigFilename;
    }
    loadConfigFile(m_osvrUserConfigFilename);
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::loadConfigFile(QString filename)
{
    char* cstr;
    std::string fname = filename.toStdString();
    cstr = new char [fname.size()+1];
    strcpy( cstr, fname.c_str() );

    std::ifstream file_id;
    file_id.open(cstr);

    Json::Reader reader;
    Json::Value value;
    if (!reader.parse (file_id, value)){
         qWarning("Couldn't open save file, creating file.");
        // new file just has default values
        saveConfigFile(filename);
    }else{
        m_osvrUser.read(value);
    }
    updateFormValues();
    return true;
}

void MainWindow::updateFormValues(){
    if ("Male" == m_osvrUser.gender())
        ui->gender->setCurrentIndex(0);
    else
        ui->gender->setCurrentIndex(1);
    ui->standingHeight->setText(QString::number(m_osvrUser.standingEyeHeight()));
    ui->seatedHeight->setText(QString::number(m_osvrUser.seatedEyeHeight()));
    ui->ipd->setText(QString::number(m_osvrUser.pupilDistance(OS)+m_osvrUser.pupilDistance(OD)));

    ui->dOsSpherical->setText(QString::number(m_osvrUser.spherical(OS)));
    ui->dOsCylindrical->setText(QString::number(m_osvrUser.cylindrical(OS)));
    ui->dOsAxis->setText(QString::number(m_osvrUser.axis(OS)));
    ui->nOsAdd->setText(QString::number(m_osvrUser.addNear(OS)));
    ui->OSdominant->setChecked(m_osvrUser.dominant(OS));

    ui->dOdSpherical->setText(QString::number(m_osvrUser.spherical(OD)));
    ui->dOdCylindrical->setText(QString::number(m_osvrUser.cylindrical(OD)));
    ui->dOdAxis->setText(QString::number(m_osvrUser.axis(OD)));
    ui->nOdAdd->setText(QString::number(m_osvrUser.addNear(OD)));
    ui->ODdominant->setChecked(m_osvrUser.dominant(OD));
}

void MainWindow::on_resetButton_clicked()
{
    updateFormValues();
}

void MainWindow::on_aboutButton_clicked()
{
    QMessageBox::information(0, QString("OSVR Control Panel Interface"), QString("Simple utility for helping you set up and configure your personal OSVR settings... For more information, visit www.osvr.com."), QMessageBox::Ok);
    // loadConfigFile(QString("start.json"));
}

void MainWindow::loadValuesFromForm(OSVRUser *oo)
{
    double ipd;

    oo->setGender(ui->gender->currentText().toStdString());

    // IPD settings
    if (!ui->ipd->text().isEmpty()){
        ipd = ui->ipd->text().toDouble();
        oo->setPupilDistance(OS,ipd/2);
        oo->setPupilDistance(OD,ipd/2);
    }

    //Left eye settings
    if (!ui->dOsSpherical->text().isEmpty())
        oo->setSpherical(OS,ui->dOsSpherical->text().toDouble());
    if (!ui->dOsCylindrical->text().isEmpty())
        oo->setCylindrical(OS,ui->dOsCylindrical->text().toDouble());
    if (!ui->dOsAxis->text().isEmpty())
        oo->setAxis(OS,ui->dOsAxis->text().toDouble());
    if (!ui->nOsAdd->text().isEmpty())
        oo->setAddNear(OS,ui->nOsAdd->text().toDouble());

    // right eye settings
    if (!ui->dOdSpherical->text().isEmpty())
        oo->setSpherical(OD,ui->dOdSpherical->text().toDouble());
    if (!ui->dOdCylindrical->text().isEmpty())
        oo->setCylindrical(OD,ui->dOdCylindrical->text().toDouble());
    if (!ui->dOdAxis->text().isEmpty())
        oo->setAxis(OD,ui->dOdAxis->text().toDouble());
    if (!ui->nOdAdd->text().isEmpty())
        oo->setAddNear(OD,ui->nOdAdd->text().toDouble());

    if(ui->ODdominant->isChecked())
        oo->setDominant(OD);
    else
        oo->setDominant(OS);

    // anthropometric settings
    if (!ui->standingHeight->text().isEmpty())
        oo->setStandingEyeHeight(ui->standingHeight->text().toDouble());
    if (!ui->seatedHeight->text().isEmpty())
        oo->setSeatedEyeHeight(ui->seatedHeight->text().toDouble());
}

void MainWindow::saveConfigFile(QString filename)
{
    char* cstr;
    std::string fname = filename.toStdString();
    cstr = new char [fname.size()+1];
    strcpy( cstr, fname.c_str() );

    Json::Value ooo;
    m_osvrUser.write(ooo);

    std::ofstream out_file;
    out_file.open(cstr);
    Json::StyledWriter styledWriter;
    out_file <<styledWriter.write(ooo);
    out_file.close();
}

void MainWindow::on_saveButton_clicked()
{
    loadValuesFromForm(&m_osvrUser);
    saveConfigFile(m_osvrUserConfigFilename);
}

void MainWindow::on_exitButton_clicked()
{
    QApplication::quit();
}

void MainWindow::on_resetYawButton_clicked()
{
    QProcess *process = new QProcess(this);
    QString file = "reset_yaw.bat";
    process->start(file);
}

void MainWindow::on_disableButton_clicked()
{
    QProcess *process = new QProcess(this);
    QString file = "disableOSVRDirect.bat";
    process->start(file);
}

void MainWindow::on_enableButton_clicked()
{
    QProcess *process = new QProcess(this);
    QString file = "enableOSVRDirect.bat";
    process->start(file);
}

// FW Update button
void MainWindow::on_updateFWButton_clicked()
{
    QMessageBox msgBox;
    QMessageBox::StandardButton reply;
    QString portName;
    QString hexFile;
    QString fileFilter = "*.hex";

    // find the OSVR HDK and get current FW version
    QString fwVersion = sendCommandWaitForResults("#?v\n");
    if (fwVersion !=  ""){
        reply = QMessageBox::question(this,tr("FW version"),
                "Current FW Version: " + fwVersion + "\nDo you wish to proceed?",
                QMessageBox::Yes|QMessageBox::No);
        if (reply==QMessageBox::No)
            return;
    }else
        return;

    // ask User for the HEX file to update with
    hexFile = QFileDialog::getOpenFileName(this,QString("Open FW Update File"),fileFilter,0);
    if (hexFile == ""){
         return; // user pressed cancel
    }
    msgBox.setText("FW hex file: "+ hexFile);
    msgBox.exec();

    sendCommandNoResult("#?b1948\n");

    msgBox.setText("This application uses the opensource dfu-programmer which can be found here: dfu-programmer.github.io \n\nAt this time your device should now be in ATMEL bootloader mode. If you haven't loaded the ATMEL drivers yet, you should do so now. You can find the drivers in this distribution in the dfu-prog-usb-1.2.2 folder. \n\nRight click on the device in the device manager and Update the Driver Softare...");
    msgBox.exec();

    atmel_erase();
    atmel_load(hexFile);
    atmel_launch();

    // Verify FW version
    fwVersion = sendCommandWaitForResults("#?v\n");
    if (fwVersion ==  ""){
        fwVersion = "Error: Cannot read FW version.";
    }
    msgBox.setText("FW Version: " + fwVersion);
    msgBox.exec();
}

void MainWindow::on_checkFWButton_clicked()
{
    // get FW version
    QString fwVersion = sendCommandWaitForResults("#?v\n");
    if (fwVersion !=  ""){
        QMessageBox msgBox;
        msgBox.setText("FW Version: " + fwVersion);
        msgBox.exec();
    }
}

void MainWindow::atmel_erase()
{
    QProcess *process = new QProcess(this);
    QString file = "dfu-programmer.exe atxmega256a3bu erase";
    process->start(file);
    QThread::sleep(3);
}

void MainWindow::atmel_load(QString fwFile)
{
    QProcess *process = new QProcess(this);
    QString file = "dfu-programmer.exe atxmega256a3bu flash \"" + fwFile + "\"";
    process->start(file);
    QThread::sleep(3);
}

void MainWindow::atmel_launch()
{
    QProcess *process = new QProcess(this);
    QString file = "dfu-programmer.exe atxmega256a3bu launch";
    process->start(file);
    QThread::sleep(5);
}



QSerialPort *MainWindow::openSerialPort(QString portName)
{
    QSerialPort *thePort = new QSerialPort(this);

    thePort->setPortName(portName);
    thePort->setBaudRate(QSerialPort::Baud57600);
    thePort->setDataBits(QSerialPort::Data8);
    thePort->setParity(QSerialPort::NoParity);
    thePort->setStopBits(QSerialPort::OneStop);
    thePort->setFlowControl(QSerialPort::NoFlowControl);
    if (thePort->open(QIODevice::ReadWrite)) {
        return thePort;
    } else {
        QMessageBox::critical(this,tr("Open port fail"),"Cannot open serial port " + portName + ".\n Please check your connections and try again.\n");
        return NULL;
    }
}

void MainWindow::writeSerialData(QSerialPort *thePort, const QByteArray &data)
{
    if (thePort->write(data)== -1){
        QMessageBox::critical(this,tr("Write serial port failure"),"Cannot write to serial port.\n Please check your connections and try again.\n");
    }
    thePort->flush();
#ifdef _WIN32
    // TODO: Find out why this doesn't work on Linux
    thePort->waitForBytesWritten(5000);
#endif
    QThread::sleep(1);
}

QSerialPort* MainWindow::findAndOpenSerialPort() {
    auto devs = osvr::usbserial::enumerate(0x1532, 0x0B00);

    int hdkcount = 0;
    for (auto &&dev : devs) hdkcount++;
    if (hdkcount == 0) { // callers need to handle this
        return NULL;
    }

    auto &&dev = *(devs.begin());
    QString portName = QString::fromStdString(dev.getPlatformSpecificPath());
    if (m_verbose) {
        std::cout << "Found serial device: " << portName.toStdString() << std::endl;
    }

    if (hdkcount > 1) {
        std::string msg = std::to_string(hdkcount) + " HDKs have been found. \
This application will only use the first connected one \
at serial port " + portName.toStdString();
        QMessageBox::critical(this,tr("Alert"), msg.c_str());
    }
    QSerialPort *thePort = openSerialPort(portName);
    return thePort;
}

QString MainWindow::sendCommandWaitForResults(QByteArray theCommand){
    QByteArray theResult="";
    QSerialPort *thePort;
    thePort = findAndOpenSerialPort();
    if (thePort){
        writeSerialData(thePort,theCommand);
        if (thePort->waitForReadyRead(5000)){
            theResult = thePort->readAll();
        }
        thePort->close();
    } else {
        QMessageBox::critical(this,tr("Alert"),"Could not retrieve results from " + theCommand + ". Check your connections and try again.");
    }
    return theResult;
}

void MainWindow::sendCommandNoResult(QByteArray theCommand){
    QSerialPort *thePort;
    thePort = findAndOpenSerialPort();
    if (thePort){
        writeSerialData(thePort,theCommand);
        thePort->close();
    }else{
        QMessageBox::critical(this,tr("Alert"),"Could not retrieve results from " + theCommand + ". Check your connections and try again.");
    }
}


void MainWindow::on_enableDisplayButton_clicked()
{
    sendCommandNoResult("#Hi\n");
}

void MainWindow::on_persistanceSlider_valueChanged(int value)
{
    /*
        sendCommandNoResult("#sp03C0A"); // 60Hz@10%
        sendCommandNoResult("#sp03C14"); // 60Hz@20%
        sendCommandNoResult("#sp03C1E"); // 60Hz@30%
        sendCommandNoResult("#sp03C28"); // 60Hz@40%
        sendCommandNoResult("#sp03C32"); // 60Hz@50%
        sendCommandNoResult("#sp03C3C"); // 60Hz@60%
        sendCommandNoResult("#sp03C4B"); // 60Hz@70%
        sendCommandNoResult("#sp03C50"); // 60Hz@80%
        sendCommandNoResult("#sp03C5A"); // 60Hz@90%
        sendCommandNoResult("#sp0F012"); // 240Hz@18%
        sendCommandNoResult("#sp0F032"); // 240Hz@50%
    */

    switch (value){
    case 0:
            // low persistence
        sendCommandNoResult("#sp03C0A");
        break;
    case 1:
            // med persistence
        sendCommandNoResult("#sp03C32");
        break;
    case 2:
            // high persistence
            sendCommandNoResult("#sp03C50");
        break;
    }
    return;
}

void MainWindow::on_rotationVectorSlider_sliderReleased()
{
    switch (ui->rotationVectorSlider->value()){
        case 0:
            // Rotation
            sendCommandNoResult("#sg0\n");
            return;
        case 1:
            // Game rotation
            sendCommandNoResult("#sg1\n");
            return;
    }
}

void MainWindow::on_disableButton_2_clicked()
{
    sendCommandNoResult("#f1s\n");
}
