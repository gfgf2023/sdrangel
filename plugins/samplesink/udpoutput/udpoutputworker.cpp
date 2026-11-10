///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <QUdpSocket>
#include <QMutexLocker>
#include <QThread>
#include <QMetaObject>
#include <QVariant>
#include <QDebug>
#include <algorithm>
#include <cstring>

#include "dsp/samplesourcefifo.h"
#include "dsp/basebandsamplesink.h"
#include "udpoutputworker.h"

UDPOutputWorker::UDPOutputWorker(SampleSourceFifo* sampleFifo, QObject* parent) :
    QObject(parent),
    m_running(false),
    m_stopRequested(false),
    m_sampleFifo(sampleFifo),
    m_udpSocket(nullptr),
    m_destAddress(QHostAddress::LocalHost),
    m_destPort(9999),
    m_sampleFormat(UDPOutputSettings::FormatComplexFloat),
    m_udpPayloadSize(1472),
    m_gain(1.0f),
    m_samplerate(48000),
    m_log2Interpolation(0),
    m_throttlems(UDPOUTPUT_THROTTLE_MS),
    m_throttleToggle(false),
    m_samplesChunkSize(0),
    m_samplesCount(0),
    m_buf(nullptr),
    m_spectrumSink(nullptr)
{
    m_payloadBuf.reserve(4 * 1024 * 1024);
    setSamplerate(m_samplerate);
}

UDPOutputWorker::~UDPOutputWorker()
{
    if (m_running) {
        stopWork();
    }
    if (m_buf) {
        delete[] m_buf;
        m_buf = nullptr;
    }
    if (m_udpSocket) {
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void UDPOutputWorker::startWork()
{
    if (m_stopRequested.load())
    {
        QMetaObject::invokeMethod(QThread::currentThread(), "quit", Qt::QueuedConnection);
        return;
    }
    QHostAddress destAddress;
    uint16_t destPort;
    {
        QMutexLocker locker(&m_settingsMutex);
        destAddress = m_destAddress;
        destPort = m_destPort;
    }
    qDebug() << "UDPOutputWorker::startWork: destination" << destAddress.toString() << ":" << destPort;
    if (!m_udpSocket) {
        m_udpSocket = new QUdpSocket(this);
    }
    // Set socket send buffer to 8 MB to accommodate high sample rates without kernel packet drops
    m_udpSocket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, QVariant(8 * 1024 * 1024));
    m_payloadBuf.clear();
    m_payloadBuf.reserve(1024 * 1024);
    m_running = true;

    // Run continuous, micro-paced streaming loop in worker thread (eliminates 50ms GUI timer microbursts)
    runLoop();

    if (m_stopRequested.load()) {
        QMetaObject::invokeMethod(QThread::currentThread(), "quit", Qt::QueuedConnection);
    }
}

void UDPOutputWorker::stopWork()
{
    qDebug() << "UDPOutputWorker::stopWork";
    m_stopRequested = true;
    m_running = false;
}

void UDPOutputWorker::setSamplerate(int samplerate)
{
    if (samplerate <= 0) {
        return;
    }

    if (m_running) {
        qWarning() << "UDPOutputWorker::setSamplerate: ignored while worker is running";
        return;
    }

    if (samplerate != m_samplerate || !m_buf)
    {
        qDebug() << "UDPOutputWorker::setSamplerate:" << samplerate;

        if (m_sampleFifo) {
            m_sampleFifo->resize(SampleSourceFifo::getSizePolicy(samplerate));
        }

        if (m_buf) {
            delete[] m_buf;
        }
        m_samplerate = samplerate;
        m_buf = new int16_t[m_samplerate * (1 << m_log2Interpolation) * 2];
        m_samplesChunkSize = (m_samplerate * m_throttlems) / 1000;
    }
}

void UDPOutputWorker::setLog2Interpolation(int log2Interpolation)
{
    if ((log2Interpolation < 0) || (log2Interpolation > 6)) {
        return;
    }

    if (m_running) {
        qWarning() << "UDPOutputWorker::setLog2Interpolation: ignored while worker is running";
        return;
    }

    if (log2Interpolation != m_log2Interpolation)
    {
        qDebug() << "UDPOutputWorker::setLog2Interpolation:" << log2Interpolation;

        if (m_buf) {
            delete[] m_buf;
        }
        m_log2Interpolation = log2Interpolation;
        m_buf = new int16_t[m_samplerate * (1 << m_log2Interpolation) * 2];
    }
}

void UDPOutputWorker::setUDPAddress(const QString& address)
{
    QMutexLocker locker(&m_settingsMutex);
    m_destAddress.setAddress(address);
}

void UDPOutputWorker::setUDPPort(uint16_t port)
{
    QMutexLocker locker(&m_settingsMutex);
    m_destPort = port;
}

void UDPOutputWorker::setSampleFormat(UDPOutputSettings::SampleFormat format)
{
    QMutexLocker locker(&m_settingsMutex);
    m_sampleFormat = format;
}

void UDPOutputWorker::setUDPPayloadSize(quint32 size)
{
    if (size >= 64 && size <= 65507) {
        QMutexLocker locker(&m_settingsMutex);
        m_udpPayloadSize = size;
    }
}

void UDPOutputWorker::setGain(float gain)
{
    QMutexLocker locker(&m_settingsMutex);
    m_gain = gain;
}

void UDPOutputWorker::connectTimer(const QTimer& timer)
{
    (void) timer;
}

void UDPOutputWorker::tick()
{
    // No-op: sample streaming is driven continuously by runLoop()
}

void UDPOutputWorker::runLoop()
{
    using clock = std::chrono::steady_clock;
    auto tStart = clock::now();
    uint64_t totalOutputSamples = 0;
    const uint64_t interpFactor = 1ULL << m_log2Interpolation;
    const uint64_t outputSampleRate = (uint64_t) m_samplerate * interpFactor;

    if (outputSampleRate == 0 || !m_sampleFifo) {
        return;
    }

    // Process in small ~2 ms chunks for smooth, gap-free streaming (eliminating 50ms microbursts)
    uint32_t baseChunkSize = (m_samplerate * 2) / 1000;
    if (baseChunkSize < 128) {
        baseChunkSize = 128;
    }
    const uint64_t baseOutputChunkSamples = (uint64_t) baseChunkSize * interpFactor;

    while (m_running)
    {
        auto now = clock::now();
        double elapsedSec = std::chrono::duration<double>(now - tStart).count();
        uint64_t expectedOutputSamples = (uint64_t)(elapsedSec * outputSampleRate);

        if (totalOutputSamples + baseOutputChunkSamples <= expectedOutputSamples)
        {
            uint32_t inputChunkSize = baseChunkSize;

            // Adaptive synchronization: gently lock reader rate to FIFO balance to prevent overrun/underrun
            if (m_sampleFifo)
            {
                float balance = m_sampleFifo->getRWBalance();
                if (balance < -0.05f) {
                    inputChunkSize = (inputChunkSize * 102) / 100; // reading lags, slightly speed up
                } else if (balance > 0.05f) {
                    inputChunkSize = (inputChunkSize * 98) / 100; // reading leads, slightly slow down
                }
            }

            unsigned int iPart1Begin = 0, iPart1End = 0, iPart2Begin = 0, iPart2End = 0;
            SampleVector& data = m_sampleFifo->getData();
            m_sampleFifo->read(inputChunkSize, iPart1Begin, iPart1End, iPart2Begin, iPart2End);

            if (iPart1Begin != iPart1End) {
                callbackPart(data, iPart1Begin, iPart1End);
            }
            if (iPart2Begin != iPart2End) {
                callbackPart(data, iPart2Begin, iPart2End);
            }

            const unsigned int actualRead = (iPart1End - iPart1Begin) + (iPart2End - iPart2Begin);
            totalOutputSamples += (uint64_t) actualRead * interpFactor;
            m_samplesCount += actualRead;
        }
        else
        {
            // Ahead of schedule: yield or sleep briefly
            uint64_t aheadSamples = (totalOutputSamples + baseOutputChunkSamples) - expectedOutputSamples;
            double aheadSec = (double) aheadSamples / outputSampleRate;
            if (aheadSec > 0.0015) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                std::this_thread::yield();
            }
        }
    }
}

void UDPOutputWorker::callbackPart(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    // Clamp input samples to signed 16-bit range [-32767, 32767] to prevent integer overflow wraparound (32768 -> -32768)
    for (auto it = data.begin() + iBegin; it != data.begin() + iEnd; ++it)
    {
        if (it->m_real > 32767) it->m_real = 32767;
        else if (it->m_real < -32767) it->m_real = -32767;
        if (it->m_imag > 32767) it->m_imag = 32767;
        else if (it->m_imag < -32767) it->m_imag = -32767;
    }

    SampleVector::iterator beginRead = data.begin() + iBegin;
    unsigned int chunkSize = iEnd - iBegin;
    unsigned int nbOutputSamples = chunkSize;

    if (m_log2Interpolation == 0)
    {
        m_interpolators.interpolate1(&beginRead, m_buf, 2 * chunkSize);
        nbOutputSamples = chunkSize;
    }
    else
    {
        nbOutputSamples = chunkSize * (1 << m_log2Interpolation);
        switch (m_log2Interpolation)
        {
        case 1:
            m_interpolators.interpolate2_cen(&beginRead, m_buf, 2 * nbOutputSamples);
            break;
        case 2:
            m_interpolators.interpolate4_cen(&beginRead, m_buf, 2 * nbOutputSamples);
            break;
        case 3:
            m_interpolators.interpolate8_cen(&beginRead, m_buf, 2 * nbOutputSamples);
            break;
        case 4:
            m_interpolators.interpolate16_cen(&beginRead, m_buf, 2 * nbOutputSamples);
            break;
        case 5:
            m_interpolators.interpolate32_cen(&beginRead, m_buf, 2 * nbOutputSamples);
            break;
        case 6:
            m_interpolators.interpolate64_cen(&beginRead, m_buf, 2 * nbOutputSamples);
            break;
        default:
            m_interpolators.interpolate1(&beginRead, m_buf, 2 * chunkSize);
            nbOutputSamples = chunkSize;
            break;
        }
    }

    feedSpectrum(m_buf, 2 * nbOutputSamples);
    convertAndSend(m_buf, nbOutputSamples);
}

void UDPOutputWorker::feedSpectrum(int16_t *buf, unsigned int bufSize)
{
    if (!m_spectrumSink) {
        return;
    }

    m_samplesVector.allocate(bufSize / 2);
    Sample16 *s16Buf = (Sample16*) buf;

    std::transform(
        s16Buf,
        s16Buf + (bufSize / 2),
        m_samplesVector.m_vector.begin(),
        [](Sample16 s) -> Sample {
            return Sample{s.m_real, s.m_imag};
        }
    );

    m_spectrumSink->feed(m_samplesVector.m_vector.begin(), m_samplesVector.m_vector.begin() + (bufSize / 2), false);
}

void UDPOutputWorker::convertAndSend(int16_t *buf, unsigned int nbComplexSamples)
{
    if (!m_udpSocket || nbComplexSamples == 0) {
        return;
    }

    QHostAddress destAddress;
    uint16_t destPort;
    UDPOutputSettings::SampleFormat sampleFormat;
    quint32 udpPayloadSize;
    float gain;
    {
        QMutexLocker locker(&m_settingsMutex);
        destAddress = m_destAddress;
        destPort = m_destPort;
        sampleFormat = m_sampleFormat;
        udpPayloadSize = m_udpPayloadSize;
        gain = m_gain;
    }

    switch (sampleFormat)
    {
    case UDPOutputSettings::FormatComplexFloat:
    {
        const float scale = gain / 32768.0f;
        const std::size_t bytesToAdd = nbComplexSamples * 2 * sizeof(float);
        std::size_t offset = m_payloadBuf.size();
        m_payloadBuf.resize(offset + bytesToAdd);
        float* dest = reinterpret_cast<float*>(&m_payloadBuf[offset]);

        for (unsigned int i = 0; i < nbComplexSamples; ++i)
        {
            dest[2 * i]     = ((float) buf[2 * i]) * scale;
            dest[2 * i + 1] = ((float) buf[2 * i + 1]) * scale;
        }
        break;
    }
    case UDPOutputSettings::FormatIQ16:
    {
        const std::size_t bytesToAdd = nbComplexSamples * 2 * sizeof(int16_t);
        std::size_t offset = m_payloadBuf.size();
        m_payloadBuf.resize(offset + bytesToAdd);
        int16_t* dest = reinterpret_cast<int16_t*>(&m_payloadBuf[offset]);

        if (gain == 1.0f)
        {
            std::memcpy(dest, buf, bytesToAdd);
        }
        else
        {
            for (unsigned int i = 0; i < nbComplexSamples; ++i)
            {
                dest[2 * i]     = (int16_t) qBound(-32768, (int)(buf[2 * i] * gain), 32767);
                dest[2 * i + 1] = (int16_t) qBound(-32768, (int)(buf[2 * i + 1] * gain), 32767);
            }
        }
        break;
    }
    case UDPOutputSettings::FormatIQ8:
    {
        const float scale = gain / 256.0f;
        const std::size_t bytesToAdd = nbComplexSamples * 2 * sizeof(int8_t);
        std::size_t offset = m_payloadBuf.size();
        m_payloadBuf.resize(offset + bytesToAdd);
        int8_t* dest = reinterpret_cast<int8_t*>(&m_payloadBuf[offset]);

        for (unsigned int i = 0; i < nbComplexSamples; ++i)
        {
            dest[2 * i]     = (int8_t) qBound(-128, (int)(buf[2 * i] * scale), 127);
            dest[2 * i + 1] = (int8_t) qBound(-128, (int)(buf[2 * i + 1] * scale), 127);
        }
        break;
    }
    case UDPOutputSettings::FormatIQ24:
    {
        const float scale = gain * 256.0f;
        const std::size_t bytesToAdd = nbComplexSamples * 6; // 3 bytes I + 3 bytes Q
        std::size_t offset = m_payloadBuf.size();
        m_payloadBuf.resize(offset + bytesToAdd);
        uint8_t* dest = reinterpret_cast<uint8_t*>(&m_payloadBuf[offset]);

        for (unsigned int i = 0; i < nbComplexSamples; ++i)
        {
            int32_t valI = qBound(-8388608, (int32_t)(buf[2 * i] * scale), 8388607);
            int32_t valQ = qBound(-8388608, (int32_t)(buf[2 * i + 1] * scale), 8388607);

            dest[6 * i + 0] = (uint8_t) (valI & 0xFF);
            dest[6 * i + 1] = (uint8_t) ((valI >> 8) & 0xFF);
            dest[6 * i + 2] = (uint8_t) ((valI >> 16) & 0xFF);

            dest[6 * i + 3] = (uint8_t) (valQ & 0xFF);
            dest[6 * i + 4] = (uint8_t) ((valQ >> 8) & 0xFF);
            dest[6 * i + 5] = (uint8_t) ((valQ >> 16) & 0xFF);
        }
        break;
    }
    }

    // Determine sample size in bytes to align datagrams to complex sample boundaries
    std::size_t sampleBytes = 2;
    switch (sampleFormat)
    {
    case UDPOutputSettings::FormatComplexFloat: sampleBytes = 8; break;
    case UDPOutputSettings::FormatIQ16:          sampleBytes = 4; break;
    case UDPOutputSettings::FormatIQ8:           sampleBytes = 2; break;
    case UDPOutputSettings::FormatIQ24:          sampleBytes = 6; break;
    }

    std::size_t payloadSize = (udpPayloadSize / sampleBytes) * sampleBytes;
    if (payloadSize == 0) {
        payloadSize = sampleBytes;
    }

    // Transmit completed datagrams using index offset (O(1) per packet, zero repeated vector::erase memmove)
    std::size_t offset = 0;
    while (m_payloadBuf.size() - offset >= payloadSize)
    {
        qint64 sent = m_udpSocket->writeDatagram(
            m_payloadBuf.data() + offset,
            (qint64) payloadSize,
            destAddress,
            destPort
        );

        if (sent < 0) {
            // Socket send buffer full (EWOULDBLOCK) or transient network congestion:
            // Stop sending in this tick, preserve remaining unwritten data for the next tick!
            break;
        }
        offset += payloadSize;
    }

    // Single erase to shift only the unwritten remainder (< payloadSize)
    if (offset > 0) {
        m_payloadBuf.erase(m_payloadBuf.begin(), m_payloadBuf.begin() + offset);
    }

    // Drop oldest data only if buffer grows excessively large (e.g. > 16 MB, indicating destination unreachable)
    if (m_payloadBuf.size() > 16 * 1024 * 1024) {
        m_payloadBuf.clear();
    }
}
