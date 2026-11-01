///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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
#include <QTime>
#include <QString>
#include <cmath>

#include "ui_udpoutputgui.h"
#include "gui/messagedialog.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/glspectrumgui.h"
#include "gui/basicdevicesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "mainspectrum/mainspectrumgui.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspcommands.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "udpoutputgui.h"

UDPOutputGui::UDPOutputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::UDPOutputGui),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_settings(),
    m_sampleRate(0),
    m_deviceCenterFrequency(0),
    m_lastEngineState(DeviceAPI::StNotStarted)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/samplesink/udpoutput/readme.md";
    QWidget *contents = getContents();
    ui->setupUi(contents);
    sizeToContents();
    setMinimumSize(m_MinimumWidth, m_MinimumHeight);
    getContents()->setStyleSheet("#UDPOutputGui { background-color: rgb(64, 64, 64); }");
    m_sampleSink = (UDPOutput*) m_deviceUISet->m_deviceAPI->getSampleSink();

    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(9, 0, pow(10, 9));

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, 8000U, 90000000U);

    m_spectrumVis = m_sampleSink->getSpectrumVis();
    m_spectrumVis->setGLSpectrum(ui->glSpectrum);
    ui->glSpectrum->setCenterFrequency(m_settings.m_centerFrequency);
    ui->glSpectrum->setSampleRate(m_settings.m_sampleRate * (1 << m_settings.m_log2Interp));
    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);
    m_sampleSink->setSpectrumGUI(ui->spectrumGUI);

    connect(&(m_deviceUISet->m_deviceAPI->getMasterTimer()), &QTimer::timeout, this, &UDPOutputGui::tick);
    connect(&m_updateTimer, &QTimer::timeout, this, &UDPOutputGui::updateHardware);
    connect(&m_statusTimer, &QTimer::timeout, this, &UDPOutputGui::updateStatus);
    m_statusTimer.start(500);

    displaySettings();
    makeUIConnections();
    m_resizer.enableChildMouseTracking();

    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &UDPOutputGui::handleInputMessages, Qt::QueuedConnection);
    connect(this, &QWidget::customContextMenuRequested, this, &UDPOutputGui::openDeviceSettingsDialog);
}

UDPOutputGui::~UDPOutputGui()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void UDPOutputGui::destroy()
{
    delete this;
}

void UDPOutputGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray UDPOutputGui::serialize() const
{
    return m_settings.serialize();
}

bool UDPOutputGui::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data)) {
        displaySettings();
        m_forceSettings = true;
        sendSettings();
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool UDPOutputGui::handleMessage(const Message& message)
{
    if (UDPOutput::MsgConfigureUDPOutput::match(message))
    {
        qDebug("UDPOutputGui::handleMessage: MsgConfigureUDPOutput");
        const UDPOutput::MsgConfigureUDPOutput& cfg = (const UDPOutput::MsgConfigureUDPOutput&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (UDPOutput::MsgStartStop::match(message))
    {
        const UDPOutput::MsgStartStop& notif = (const UDPOutput::MsgStartStop&) message;
        qDebug("UDPOutputGui::handleMessage: MsgStartStop: %s", notif.getStartStop() ? "start" : "stop");
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);
        return true;
    }

    return false;
}

void UDPOutputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            qDebug("UDPOutputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu",
                    notif->getSampleRate(), notif->getCenterFrequency());
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            updateSampleRateAndFrequency();
            delete message;
        }
        else
        {
            if (!handleMessage(*message)) {
                qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
            }
            delete message;
        }
    }
}

void UDPOutputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)(m_sampleRate * (1 << m_settings.m_log2Interp)) / 1000.0f));
}

void UDPOutputGui::displaySettings()
{
    blockApplySettings(true);

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->sampleRate->setValue(m_settings.m_sampleRate);
    ui->interp->setCurrentIndex(m_settings.m_log2Interp);

    ui->address->setText(m_settings.m_udpAddress);
    ui->port->setValue(m_settings.m_udpPort);
    ui->format->setCurrentIndex((int) m_settings.m_sampleFormat);

    QString payloadStr = QString::number(m_settings.m_udpPayloadSize);
    int pIdx = ui->payloadSize->findText(payloadStr, Qt::MatchStartsWith);
    if (pIdx >= 0) {
        ui->payloadSize->setCurrentIndex(pIdx);
    }

    int gainPct = (int) (m_settings.m_gain * 100.0f);
    ui->gainSlider->setValue(gainPct);
    ui->gainText->setText(QString("%1%").arg(gainPct));

    setTitle(m_settings.m_title);
    getDeviceUISet()->m_mainSpectrumGUI->setTitle(m_settings.m_title);

    blockApplySettings(false);
}

void UDPOutputGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void UDPOutputGui::updateHardware()
{
    qDebug() << "UDPOutputGui::updateHardware";
    UDPOutput::MsgConfigureUDPOutput* message = UDPOutput::MsgConfigureUDPOutput::create(m_settings, m_settingsKeys, m_forceSettings);
    m_sampleSink->getInputMessageQueue()->push(message);
    m_forceSettings = false;
    m_settingsKeys.clear();
    m_updateTimer.stop();
}

void UDPOutputGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state();

    if (m_lastEngineState != state)
    {
        switch(state)
        {
            case DeviceAPI::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DeviceAPI::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DeviceAPI::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DeviceAPI::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                MessageDialog::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void UDPOutputGui::on_centerFrequency_changed(quint64 value)
{
    if (!m_doApplySettings) return;
    m_settings.m_centerFrequency = value * 1000;
    ui->glSpectrum->setCenterFrequency(m_settings.m_centerFrequency);
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void UDPOutputGui::on_sampleRate_changed(quint64 value)
{
    if (!m_doApplySettings) return;
    m_settings.m_sampleRate = value;
    ui->glSpectrum->setSampleRate(m_settings.m_sampleRate * (1 << m_settings.m_log2Interp));
    m_settingsKeys.append("sampleRate");
    sendSettings();
}

void UDPOutputGui::on_interp_currentIndexChanged(int index)
{
    if (!m_doApplySettings || index < 0) return;
    m_settings.m_log2Interp = index;
    ui->glSpectrum->setSampleRate(m_settings.m_sampleRate * (1 << m_settings.m_log2Interp));
    m_settingsKeys.append("log2Interp");
    sendSettings();
}

void UDPOutputGui::on_address_editingFinished()
{
    if (!m_doApplySettings) return;
    m_settings.m_udpAddress = ui->address->text().trimmed();
    m_settingsKeys.append("udpAddress");
    sendSettings();
}

void UDPOutputGui::on_port_valueChanged(int value)
{
    if (!m_doApplySettings) return;
    m_settings.m_udpPort = (uint16_t) value;
    m_settingsKeys.append("udpPort");
    sendSettings();
}

void UDPOutputGui::on_format_currentIndexChanged(int index)
{
    if (!m_doApplySettings || index < 0) return;
    m_settings.m_sampleFormat = (UDPOutputSettings::SampleFormat) index;
    m_settingsKeys.append("sampleFormat");
    sendSettings();
}

void UDPOutputGui::on_payloadSize_currentIndexChanged(int index)
{
    if (!m_doApplySettings || index < 0) return;
    QString txt = ui->payloadSize->itemText(index);
    int spaceIdx = txt.indexOf(' ');
    if (spaceIdx > 0) {
        txt = txt.left(spaceIdx);
    }
    m_settings.m_udpPayloadSize = txt.toUInt();
    m_settingsKeys.append("udpPayloadSize");
    sendSettings();
}

void UDPOutputGui::on_gainSlider_valueChanged(int value)
{
    ui->gainText->setText(QString("%1%").arg(value));
    if (!m_doApplySettings) return;
    m_settings.m_gain = (float) value / 100.0f;
    m_settingsKeys.append("gain");
    sendSettings();
}

void UDPOutputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        UDPOutput::MsgStartStop *message = UDPOutput::MsgStartStop::create(checked);
        m_sampleSink->getInputMessageQueue()->push(message);
    }
}

void UDPOutputGui::tick()
{
}

void UDPOutputGui::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
    {
        BasicDeviceSettingsDialog dialog(this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setTitle(m_settings.m_title);
        dialog.setDefaultTitle(getDefaultTitle());

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        if (dialog.result() == QDialog::Accepted)
        {
            m_settings.m_title = dialog.getTitle();
            setTitle(m_settings.m_title);
            getDeviceUISet()->m_mainSpectrumGUI->setTitle(m_settings.m_title);
            m_settings.m_useReverseAPI = dialog.useReverseAPI();
            m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
            m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
            m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
            m_settingsKeys.append("title");
            m_settingsKeys.append("useReverseAPI");
            m_settingsKeys.append("reverseAPIAddress");
            m_settingsKeys.append("reverseAPIPort");
            m_settingsKeys.append("reverseAPIDeviceIndex");

            sendSettings();
        }
    }

    resetContextMenuType();
}

void UDPOutputGui::makeUIConnections()
{
    connect(ui->centerFrequency, &ValueDial::changed, this, &UDPOutputGui::on_centerFrequency_changed);
    connect(ui->sampleRate, &ValueDial::changed, this, &UDPOutputGui::on_sampleRate_changed);
    connect(ui->startStop, &ButtonSwitch::toggled, this, &UDPOutputGui::on_startStop_toggled);
    connect(ui->interp, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UDPOutputGui::on_interp_currentIndexChanged);
    connect(ui->address, &QLineEdit::editingFinished, this, &UDPOutputGui::on_address_editingFinished);
    connect(ui->port, QOverload<int>::of(&QSpinBox::valueChanged), this, &UDPOutputGui::on_port_valueChanged);
    connect(ui->format, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UDPOutputGui::on_format_currentIndexChanged);
    connect(ui->payloadSize, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UDPOutputGui::on_payloadSize_currentIndexChanged);
    connect(ui->gainSlider, &QSlider::valueChanged, this, &UDPOutputGui::on_gainSlider_valueChanged);
}
