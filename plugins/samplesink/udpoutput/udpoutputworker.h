///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef PLUGINS_SAMPLESINK_UDPOUTPUT_UDPOUTPUTWORKER_H_
#define PLUGINS_SAMPLESINK_UDPOUTPUT_UDPOUTPUTWORKER_H_

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QHostAddress>
#include <vector>
#include <stdint.h>

#include "dsp/interpolators.h"
#include "util/incrementalvector.h"
#include "udpoutputsettings.h"

#define UDPOUTPUT_THROTTLE_MS 50

class QUdpSocket;
class SampleSourceFifo;
class BasebandSampleSink;

class UDPOutputWorker : public QObject
{
    Q_OBJECT

public:
    UDPOutputWorker(SampleSourceFifo* sampleFifo, QObject* parent = nullptr);
    ~UDPOutputWorker();

    void setSamplerate(int samplerate);
    void setLog2Interpolation(int log2Interpolation);
    void setUDPAddress(const QString& address);
    void setUDPPort(uint16_t port);
    void setSampleFormat(UDPOutputSettings::SampleFormat format);
    void setUDPPayloadSize(quint32 size);
    void setGain(float gain);
    void setSpectrumSink(BasebandSampleSink *spectrumSink) { m_spectrumSink = spectrumSink; }
    void connectTimer(const QTimer& timer);

    bool isRunning() const { return m_running; }
    std::size_t getSamplesCount() const { return m_samplesCount; }
    void setSamplesCount(std::size_t count) { m_samplesCount = count; }

public slots:
    void startWork();
    void stopWork();

private slots:
    void tick();

private:
#pragma pack(push, 1)
    struct Sample16
    {
        int16_t m_real;
        int16_t m_imag;
    };
#pragma pack(pop)

    volatile bool m_running;

    SampleSourceFifo* m_sampleFifo;
    QUdpSocket* m_udpSocket;
    QHostAddress m_destAddress;
    uint16_t m_destPort;
    UDPOutputSettings::SampleFormat m_sampleFormat;
    quint32 m_udpPayloadSize;
    float m_gain;

    int m_samplerate;
    int m_log2Interpolation;
    int m_throttlems;
    bool m_throttleToggle;
    unsigned int m_samplesChunkSize;
    std::size_t m_samplesCount;
    QElapsedTimer m_elapsedTimer;

    Interpolators<qint16, SDR_TX_SAMP_SZ, 16> m_interpolators;
    int16_t* m_buf;
    std::vector<char> m_payloadBuf;

    BasebandSampleSink* m_spectrumSink;
    IncrementalVector<Sample> m_samplesVector;

    void callbackPart(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    void feedSpectrum(int16_t *buf, unsigned int bufSize);
    void convertAndSend(int16_t *buf, unsigned int nbComplexSamples);
};

#endif /* PLUGINS_SAMPLESINK_UDPOUTPUT_UDPOUTPUTWORKER_H_ */
