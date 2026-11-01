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

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "udpoutputsettings.h"

UDPOutputSettings::UDPOutputSettings()
{
    resetToDefaults();
}

void UDPOutputSettings::resetToDefaults()
{
    m_title = "UDPSink";
    m_centerFrequency = 435000000ULL;
    m_sampleRate = 48000ULL;
    m_log2Interp = 0;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_sampleFormat = FormatComplexFloat;
    m_udpPayloadSize = 1472;
    m_gain = 1.0f;
    m_spectrumGUI = nullptr;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray UDPOutputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeU64(1, m_centerFrequency);
    s.writeU64(2, m_sampleRate);
    s.writeU32(3, m_log2Interp);

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    s.writeString(5, m_udpAddress);
    s.writeU32(6, m_udpPort);
    s.writeS32(7, (int32_t) m_sampleFormat);
    s.writeU32(8, m_udpPayloadSize);
    s.writeFloat(9, m_gain);

    s.writeBool(10, m_useReverseAPI);
    s.writeString(11, m_reverseAPIAddress);
    s.writeU32(12, m_reverseAPIPort);
    s.writeU32(13, m_reverseAPIDeviceIndex);
    s.writeString(14, m_title);

    return s.final();
}

bool UDPOutputSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t uintval;
        int32_t intval;

        d.readU64(1, &m_centerFrequency, 435000000ULL);
        d.readU64(2, &m_sampleRate, 48000ULL);
        d.readU32(3, &m_log2Interp, 0);

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readString(5, &m_udpAddress, "127.0.0.1");
        d.readU32(6, &uintval, 9999);
        m_udpPort = (uintval > 0 && uintval <= 65535) ? (uint16_t) uintval : 9999;

        d.readS32(7, &intval, (int32_t) FormatComplexFloat);
        if (intval >= FormatComplexFloat && intval <= FormatIQ24) {
            m_sampleFormat = (SampleFormat) intval;
        } else {
            m_sampleFormat = FormatComplexFloat;
        }

        d.readU32(8, &m_udpPayloadSize, 1472);
        if (m_udpPayloadSize < 64 || m_udpPayloadSize > 65507) {
            m_udpPayloadSize = 1472;
        }

        d.readFloat(9, &m_gain, 1.0f);

        d.readBool(10, &m_useReverseAPI, false);
        d.readString(11, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(12, &uintval, 8888);
        m_reverseAPIPort = (uintval > 1023 && uintval <= 65535) ? (uint16_t) uintval : 8888;

        d.readU32(13, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;
        d.readString(14, &m_title, "UDPSink");
        return true;
    }

    resetToDefaults();
    return false;
}

void UDPOutputSettings::applySettings(const QStringList& settingsKeys, const UDPOutputSettings& settings)
{
    if (settingsKeys.isEmpty())
    {
        *this = settings;
        return;
    }

    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("sampleRate")) {
        m_sampleRate = settings.m_sampleRate;
    }
    if (settingsKeys.contains("log2Interp")) {
        m_log2Interp = settings.m_log2Interp;
    }
    if (settingsKeys.contains("udpAddress")) {
        m_udpAddress = settings.m_udpAddress;
    }
    if (settingsKeys.contains("udpPort")) {
        m_udpPort = settings.m_udpPort;
    }
    if (settingsKeys.contains("sampleFormat")) {
        m_sampleFormat = settings.m_sampleFormat;
    }
    if (settingsKeys.contains("udpPayloadSize")) {
        m_udpPayloadSize = settings.m_udpPayloadSize;
    }
    if (settingsKeys.contains("gain")) {
        m_gain = settings.m_gain;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
}

QString UDPOutputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    QString s;
    if (force || settingsKeys.contains("centerFrequency")) {
        s += QString("centerFrequency: %1 ").arg(m_centerFrequency);
    }
    if (force || settingsKeys.contains("sampleRate")) {
        s += QString("sampleRate: %1 ").arg(m_sampleRate);
    }
    if (force || settingsKeys.contains("log2Interp")) {
        s += QString("log2Interp: %1 ").arg(m_log2Interp);
    }
    if (force || settingsKeys.contains("udpAddress")) {
        s += QString("udpAddress: %1 ").arg(m_udpAddress);
    }
    if (force || settingsKeys.contains("udpPort")) {
        s += QString("udpPort: %1 ").arg(m_udpPort);
    }
    if (force || settingsKeys.contains("sampleFormat")) {
        s += QString("sampleFormat: %1 ").arg((int)m_sampleFormat);
    }
    if (force || settingsKeys.contains("udpPayloadSize")) {
        s += QString("udpPayloadSize: %1 ").arg(m_udpPayloadSize);
    }
    if (force || settingsKeys.contains("gain")) {
        s += QString("gain: %1 ").arg(m_gain);
    }
    return s;
}
