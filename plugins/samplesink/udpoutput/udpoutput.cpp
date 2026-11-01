///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QDebug>
#include <QThread>

#include "SWGDeviceState.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/basebandsamplesink.h"
#include "device/deviceapi.h"

#include "udpoutput.h"
#include "udpoutputworker.h"

MESSAGE_CLASS_DEFINITION(UDPOutput::MsgConfigureUDPOutput, Message)
MESSAGE_CLASS_DEFINITION(UDPOutput::MsgStartStop, Message)

UDPOutput::UDPOutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_spectrumVis(SDR_TX_SCALEF),
    m_worker(nullptr),
    m_workerThread(nullptr),
    m_running(false),
    m_deviceDescription("UDPOutput"),
    m_masterTimer(deviceAPI->getMasterTimer())
{
    m_deviceAPI->setNbSinkStreams(1);
}

UDPOutput::~UDPOutput()
{
    stop();
}

void UDPOutput::destroy()
{
    delete this;
}

void UDPOutput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool UDPOutput::start()
{
    if (m_running) {
        return true;
    }

    qDebug() << "UDPOutput::start";

    m_workerThread = new QThread();
    m_workerThread->setObjectName("UDPOutputWorker");
    m_worker = new UDPOutputWorker(&m_sampleSourceFifo);
    m_worker->moveToThread(m_workerThread);

    QObject::connect(m_workerThread, &QThread::started, m_worker, &UDPOutputWorker::startWork);
    QObject::connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    QObject::connect(m_workerThread, &QThread::finished, m_workerThread, &QThread::deleteLater);

    m_worker->setSpectrumSink(&m_spectrumVis);
    m_worker->setSamplerate(m_settings.m_sampleRate);
    m_worker->setLog2Interpolation(m_settings.m_log2Interp);
    m_worker->setUDPAddress(m_settings.m_udpAddress);
    m_worker->setUDPPort(m_settings.m_udpPort);
    m_worker->setSampleFormat(m_settings.m_sampleFormat);
    m_worker->setUDPPayloadSize(m_settings.m_udpPayloadSize);
    m_worker->setGain(m_settings.m_gain);

    m_workerThread->start();
    m_running = true;

    // Notify DSP engine and baseband channel modulators of active sample rate
    auto *notif = new DSPSignalNotification(m_settings.m_sampleRate, m_settings.m_centerFrequency);
    m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

    qDebug() << "UDPOutput::start: started";
    return true;
}

void UDPOutput::stop()
{
    if (!m_running) {
        return;
    }

    qDebug() << "UDPOutput::stop";
    m_running = false;

    if (m_workerThread)
    {
        m_worker->stopWork();
        m_workerThread->quit();
        m_workerThread->wait();
        m_worker = nullptr;
        m_workerThread = nullptr;
    }
}

QByteArray UDPOutput::serialize() const
{
    return m_settings.serialize();
}

bool UDPOutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureUDPOutput* message = MsgConfigureUDPOutput::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureUDPOutput* messageToGUI = MsgConfigureUDPOutput::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& UDPOutput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int UDPOutput::getSampleRate() const
{
    return m_settings.m_sampleRate;
}

quint64 UDPOutput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void UDPOutput::setCenterFrequency(qint64 centerFrequency)
{
    UDPOutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureUDPOutput* message = MsgConfigureUDPOutput::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureUDPOutput* messageToGUI = MsgConfigureUDPOutput::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool UDPOutput::handleMessage(const Message& message)
{
    if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "UDPOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine()) {
                m_deviceAPI->startDeviceEngine();
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine();
        }

        return true;
    }
    else if (MsgConfigureUDPOutput::match(message))
    {
        MsgConfigureUDPOutput& conf = (MsgConfigureUDPOutput&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }

    return false;
}

void UDPOutput::applySettings(const UDPOutputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "UDPOutput::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
    bool forwardChange = false;
    bool needRestart = false;

    if (force || settingsKeys.contains("centerFrequency"))
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;
        forwardChange = true;
    }

    if (force || settingsKeys.contains("sampleRate"))
    {
        m_settings.m_sampleRate = settings.m_sampleRate;
        needRestart = true;
        forwardChange = true;
    }

    if (force || settingsKeys.contains("log2Interp"))
    {
        m_settings.m_log2Interp = settings.m_log2Interp;
        needRestart = true;
        forwardChange = true;
    }

    if (force || settingsKeys.contains("udpAddress"))
    {
        m_settings.m_udpAddress = settings.m_udpAddress;
        if (m_worker) {
            m_worker->setUDPAddress(m_settings.m_udpAddress);
        }
    }

    if (force || settingsKeys.contains("udpPort"))
    {
        m_settings.m_udpPort = settings.m_udpPort;
        if (m_worker) {
            m_worker->setUDPPort(m_settings.m_udpPort);
        }
    }

    if (force || settingsKeys.contains("sampleFormat"))
    {
        m_settings.m_sampleFormat = settings.m_sampleFormat;
        if (m_worker) {
            m_worker->setSampleFormat(m_settings.m_sampleFormat);
        }
    }

    if (force || settingsKeys.contains("udpPayloadSize"))
    {
        m_settings.m_udpPayloadSize = settings.m_udpPayloadSize;
        if (m_worker) {
            m_worker->setUDPPayloadSize(m_settings.m_udpPayloadSize);
        }
    }

    if (force || settingsKeys.contains("gain"))
    {
        m_settings.m_gain = settings.m_gain;
        if (m_worker) {
            m_worker->setGain(m_settings.m_gain);
        }
    }

    if (needRestart && m_running)
    {
        stop();
        start();
    }

    if (forwardChange)
    {
        qDebug("UDPOutput::applySettings: forward: m_centerFrequency: %llu m_sampleRate: %llu m_log2Interp: %d",
                m_settings.m_centerFrequency,
                m_settings.m_sampleRate,
                m_settings.m_log2Interp);
        DSPSignalNotification *notif = new DSPSignalNotification(m_settings.m_sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }
}

int UDPOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int UDPOutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgStartStop *messagetoGui = MsgStartStop::create(run);
        m_guiMessageQueue->push(messagetoGui);
    }

    return 200;
}
