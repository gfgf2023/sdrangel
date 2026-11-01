///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
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

#ifndef PLUGINS_SAMPLESINK_UDPOUTPUT_UDPOUTPUT_H_
#define PLUGINS_SAMPLESINK_UDPOUTPUT_UDPOUTPUT_H_

#include <QString>
#include <QTimer>

#include "dsp/devicesamplesink.h"
#include "dsp/spectrumvis.h"
#include "udpoutputsettings.h"

class QThread;
class UDPOutputWorker;
class DeviceAPI;
class Serializable;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class UDPOutput : public DeviceSampleSink
{
public:
    class MsgConfigureUDPOutput : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const UDPOutputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureUDPOutput* create(const UDPOutputSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureUDPOutput(settings, settingsKeys, force);
        }

    private:
        UDPOutputSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureUDPOutput(const UDPOutputSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    UDPOutput(DeviceAPI *deviceAPI);
    virtual ~UDPOutput();
    virtual void destroy();

    virtual void init();
    virtual bool start();
    virtual void stop();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual void setSampleRate(int sampleRate) { (void) sampleRate; }
    virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    virtual bool handleMessage(const Message& message);

    void applySettings(const UDPOutputSettings& settings, const QList<QString>& settingsKeys, bool force);

    SpectrumVis* getSpectrumVis() { return &m_spectrumVis; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_settings.setSpectrumGUI(spectrumGUI); }

    int webapiRunGet(SWGSDRangel::SWGDeviceState& response, QString& errorMessage);
    int webapiRun(bool run, SWGSDRangel::SWGDeviceState& response, QString& errorMessage);

private:
    DeviceAPI*          m_deviceAPI;
    UDPOutputSettings   m_settings;
    SpectrumVis         m_spectrumVis;
    UDPOutputWorker*    m_worker;
    QThread*            m_workerThread;
    bool                m_running;
    QString             m_deviceDescription;
    const QTimer&       m_masterTimer;
};

#endif /* PLUGINS_SAMPLESINK_UDPOUTPUT_UDPOUTPUT_H_ */
