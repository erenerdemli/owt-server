/*
 * Copyright 2017 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to the
 * source code ("Material") are owned by Intel Corporation or its suppliers or
 * licensors. Title to the Material remains with Intel Corporation or its suppliers
 * and licensors. The Material contains trade secrets and proprietary and
 * confidential information of Intel or its suppliers and licensors. The Material
 * is protected by worldwide copyright and trade secret laws and treaty provisions.
 * No part of the Material may be used, copied, reproduced, modified, published,
 * uploaded, posted, transmitted, distributed, or disclosed in any way without
 * Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery of
 * the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be express
 * and approved by Intel in writing.
 */

#include "AudioFrameConstructor.h"
#include "AudioUtilities.h"

#include <rtputils.h>

using namespace erizo;

namespace woogeen_base {

DEFINE_LOGGER(AudioFrameConstructor, "woogeen.AudioFrameConstructor");

AudioFrameConstructor::AudioFrameConstructor()
  : m_enabled(true)
  , m_transport(nullptr)
{
    sinkfbSource_ = this;
    fbSink_ = nullptr;
}

AudioFrameConstructor::~AudioFrameConstructor()
{
    unbindTransport();
}

void AudioFrameConstructor::bindTransport(erizo::MediaSource* source, erizo::FeedbackSink* fbSink)
{
    boost::unique_lock<boost::shared_mutex> lock(m_transport_mutex);
    m_transport = source;
    m_transport->setAudioSink(this);
    fbSink_ = fbSink;
}

void AudioFrameConstructor::unbindTransport()
{
    boost::unique_lock<boost::shared_mutex> lock(m_transport_mutex);
    if (m_transport) {
        m_transport->setAudioSink(nullptr);
        fbSink_ = nullptr;
        m_transport = nullptr;
    }
}

int AudioFrameConstructor::deliverVideoData(char* packet, int len)
{
    assert(false);
    return 0;
}

int AudioFrameConstructor::deliverAudioData(char* buf, int len)
{
    FrameFormat frameFormat;
    Frame frame;
    memset(&frame, 0, sizeof(frame));
    RTPHeader* head = (RTPHeader*) buf;

    frameFormat = getAudioFrameFormat(head->getPayloadType());
    if (frameFormat == FRAME_FORMAT_UNKNOWN)
        return 0;

    frame.additionalInfo.audio.sampleRate = getAudioSampleRate(frameFormat);
    frame.additionalInfo.audio.channels = getAudioChannels(frameFormat);

    frame.format = frameFormat;
    frame.payload = reinterpret_cast<uint8_t*>(buf);
    frame.length = len;
    frame.timeStamp = head->getTimestamp();
    frame.additionalInfo.audio.isRtpPacket = 1;

    if (m_enabled) {
        deliverFrame(frame);
    }
    return len;
}

void AudioFrameConstructor::onFeedback(const FeedbackMsg& msg)
{
    if (msg.type == woogeen_base::AUDIO_FEEDBACK) {
        boost::shared_lock<boost::shared_mutex> lock(m_transport_mutex);
        if (msg.cmd == RTCP_PACKET && fbSink_)
            fbSink_->deliverFeedback(const_cast<char*>(msg.data.rtcp.buf), msg.data.rtcp.len);
    }
}

}