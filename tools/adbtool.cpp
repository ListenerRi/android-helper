#include "adbtool.h"
#include <QDebug>

AdbTool::AdbTool(QObject *parent) : QObject(parent)
{
    // 这个process不获取信息, 仅用来确保adb servier已经启动
    QProcess::execute("adb", QStringList("devices"));

    m_adbProcess.setProgram("adb");
    connect(&m_adbProcess, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &AdbTool::handleResult);
    connect(&m_adbProcess, &QProcess::errorOccurred, this, &AdbTool::onErrorOccurred);
}

void AdbTool::startProcess(AdbTool::ProcessType processType)
{
    startProcess(processType, QString(""));
}

// 按需要执行adb
void AdbTool::startProcess(AdbTool::ProcessType processType, QString deviceSerialNumber)
{
    m_processType = processType;

    switch (m_processType) {
    case DetectDevice:
        doStart("devices", deviceSerialNumber);
        break;
    case GetDeviceDetailInfo:
        doStart("shell getprop", deviceSerialNumber);
        break;
    default:
        qDebug() << "(AdbTool::startProcess) case to default!";
        break;
    }
}

void AdbTool::doStart(QString arg, QString deviceSerialNumber)
{
    QStringList args;
    if (deviceSerialNumber.length() != 0) {
        args << "-s" << deviceSerialNumber;
    }
    // argument这个list中不能有空格!!!!!!!!!
    args << arg.split(" ", QString::SkipEmptyParts);

    qDebug() << "(AdbTool::doStart) start adb with arg:" << args;
    m_adbProcess.setArguments(args);
    m_adbProcess.start();
    m_adbProcess.waitForFinished();
}

void AdbTool::handleResult(int exitCode, QProcess::ExitStatus exitStatus)
{
    // 获取输出信息
    QByteArray result = m_adbProcess.readAllStandardOutput();
    QString resultString(result);
    qDebug() << "(AdbTool::handleResult) adb output:" << resultString;

    // 如果adb程序正常运行按需要处理数据
    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        switch (m_processType) {
        case DetectDevice:
            handleConnectInfo(resultString);
            break;
        case GetDeviceDetailInfo:
            handleDeviceDetailInfo(resultString);
            break;
        default:
            qDebug() << "(AdbTool::handleResult) case to default!";
            break;
        }
    } else {
        qDebug() << "(AdbTool::handleResult) adb error output:" << QString(m_adbProcess.readAllStandardError());
        qDebug() << "(AdbTool::handleResult) adb exec failed!!!!" << "exitCode: " << exitCode << "exitStatus: " << exitStatus;
        emit processExecFailed(m_processType);
    }
}

void AdbTool::onErrorOccurred(QProcess::ProcessError error)
{
    qDebug() << "(AdbTool::onErrorOccurred) adb exec failed!!!!" << error;
    emit processExecFailed(m_processType);
}

void AdbTool::handleConnectInfo(QString resultString)
{
    QStringList resultStringList = resultString.split("\n", QString::SkipEmptyParts);

    // 移除第一个item, 这是adb devices命令必定会输出的第一行提示信息
    resultStringList.removeFirst();

    tryToRemoveExtraInfo(resultStringList);

    // 激活信号并传递获取到的信息
    emit deviceConnectInfoReady(resultStringList);
}

void AdbTool::tryToRemoveExtraInfo(QStringList &resultStringList)
{
    // 如果adb的server第一次运行会多输出两行以"*"开头的提示信息
    if (resultStringList.count() >= 2 && static_cast<QString>(resultStringList.at(0)).startsWith("*")) {
        qDebug() << "(AdbTool::tryToRemoveExtraInfo) adb server is first time to run, remove two extra info";
        // 移除两行提示信息
        resultStringList.removeFirst();
        resultStringList.removeFirst();
    }
}

void AdbTool::handleDeviceDetailInfo(QString resultString)
{
    // 有些设备的build.prop文件中的换行符会包含无用的"\r"
    resultString = resultString.remove("\r");
    QStringList resultStringList = resultString.split("\n", QString::SkipEmptyParts);

    tryToRemoveExtraInfo(resultStringList);

    QMap<QString, QString> resultStringMap;
    foreach (QString item, resultStringList) {
        QStringList itemList = item.split(":");
        QString key, value;
        key = QString(itemList.at(0)).trimmed().mid(1);
        value = QString(itemList.at(1)).trimmed().mid(1);
        key.chop(1);
        value.chop(1);
        resultStringMap.insert(key, value);
    }
    qDebug() << "(AdbTool::handleDeviceDetailInfo) device info map:" << resultStringMap;
    emit deviceDetailInfoReady(resultStringMap);
}
