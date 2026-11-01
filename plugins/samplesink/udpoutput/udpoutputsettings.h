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

#ifndef PLUGINS_SAMPLESINK_UDPOUTPUT_UDPOUTPUTSETTINGS_H_
#define PLUGINS_SAMPLESINK_UDPOUTPUT_UDPOUTPUTSETTINGS_H_

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <stdint.h>

class Serializable;

struct UDPOutputSettings
{
    enum SampleFormat {
        FormatComplexFloat = 0, //!< 32-bit float interleaved I/Q (GNU Radio Complex)
        FormatIQ16 = 1,         //!< 16-bit signed integer interleaved I/Q (Short / S16LE)
        FormatIQ8 = 2,          //!< 8-bit signed integer interleaved I/Q (Byte / S8)
        FormatIQ24 = 3          //!< 24-bit signed integer interleaved I/Q
    };

    QString      m_title;
    quint64      m_centerFrequency;
    quint64      m_sampleRate;
    quint32      m_log2Interp;
    QString      m_udpAddress;
    uint16_t     m_udpPort;
    SampleFormat m_sampleFormat;
    quint32      m_udpPayloadSize;
    float        m_gain;
    Serializable *m_spectrumGUI;
    bool         m_useReverseAPI;
    QString      m_reverseAPIAddress;
    uint16_t     m_reverseAPIPort;
    uint16_t     m_reverseAPIDeviceIndex;

    UDPOutputSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    void applySettings(const QStringList& settingsKeys, const UDPOutputSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* PLUGINS_SAMPLESINK_UDPOUTPUT_UDPOUTPUTSETTINGS_H_ */
