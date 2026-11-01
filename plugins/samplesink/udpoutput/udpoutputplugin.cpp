///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#include <QtPlugin>

#include "plugin/pluginapi.h"

#ifdef SERVER_MODE
#include "udpoutput.h"
#else
#include "udpoutputgui.h"
#endif
#include "udpoutputplugin.h"

const PluginDescriptor UDPOutputPlugin::m_pluginDescriptor = {
    QStringLiteral("UDPOutput"),
    QStringLiteral("UDP Sink Output for GNU Radio"),
    QStringLiteral("8.0.0"),
    QStringLiteral("(c) Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "UDPOutput";
static constexpr const char* const m_deviceTypeID = UDPOUTPUT_DEVICE_TYPE_ID;

UDPOutputPlugin::UDPOutputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& UDPOutputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void UDPOutputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

void UDPOutputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) {
        return;
    }

    originDevices.append(OriginDevice(
        "UDPSink",
        m_hardwareID,
        QString(),
        0, // Sequence
        0, // nb Rx
        1  // nb Tx
    ));

    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices UDPOutputPlugin::enumSampleSinks(const OriginDevices& originDevices)
{
    SamplingDevices result;

    for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            result.append(SamplingDevice(
                it->displayableName,
                it->hardwareId,
                m_deviceTypeID,
                it->serial,
                it->sequence,
                PluginInterface::SamplingDevice::BuiltInDevice,
                PluginInterface::SamplingDevice::StreamSingleTx,
                1,
                0
            ));
        }
    }

    return result;
}

#ifdef SERVER_MODE
DeviceGUI* UDPOutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sinkId;
    (void) widget;
    (void) deviceUISet;
    return nullptr;
}
#else
DeviceGUI* UDPOutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if (sinkId == m_deviceTypeID)
    {
        UDPOutputGui* gui = new UDPOutputGui(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return nullptr;
    }
}
#endif

DeviceSampleSink* UDPOutputPlugin::createSampleSinkPluginInstance(const QString& sinkId, DeviceAPI *deviceAPI)
{
    if (sinkId == m_deviceTypeID)
    {
        UDPOutput* output = new UDPOutput(deviceAPI);
        return output;
    }
    else
    {
        return nullptr;
    }
}
