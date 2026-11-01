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

#ifndef PLUGINS_SAMPLESINK_UDPOUTPUT_UDPOUTPUTGUI_H_
#define PLUGINS_SAMPLESINK_UDPOUTPUT_UDPOUTPUTGUI_H_

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"
#include "udpoutput.h"
#include "udpoutputsettings.h"

class DeviceUISet;
class SpectrumVis;

namespace Ui {
    class UDPOutputGui;
}

class UDPOutputGui : public DeviceGUI
{
    Q_OBJECT

public:
    explicit UDPOutputGui(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
    virtual ~UDPOutputGui();
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
    Ui::UDPOutputGui* ui;

    bool m_doApplySettings;
    bool m_forceSettings;
    UDPOutputSettings m_settings;
    QList<QString> m_settingsKeys;
    QTimer m_updateTimer;
    QTimer m_statusTimer;
    UDPOutput* m_sampleSink;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency;
    int m_lastEngineState;
    MessageQueue m_inputMessageQueue;
    SpectrumVis* m_spectrumVis;

    static const int m_MinimumWidth = 380;
    static const int m_MinimumHeight = 420;

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void displaySettings();
    void sendSettings();
    void updateSampleRateAndFrequency();
    bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
    void on_centerFrequency_changed(quint64 value);
    void on_sampleRate_changed(quint64 value);
    void on_startStop_toggled(bool checked);
    void on_interp_currentIndexChanged(int index);
    void on_address_editingFinished();
    void on_port_valueChanged(int value);
    void on_format_currentIndexChanged(int index);
    void on_payloadSize_currentIndexChanged(int index);
    void on_gainSlider_valueChanged(int value);
    void openDeviceSettingsDialog(const QPoint& p);
    void updateHardware();
    void updateStatus();
    void tick();
};

#endif /* PLUGINS_SAMPLESINK_UDPOUTPUT_UDPOUTPUTGUI_H_ */
