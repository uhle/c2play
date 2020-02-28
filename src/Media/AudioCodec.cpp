/*
*
* Copyright (C) 2016 OtherCrashOverride@users.noreply.github.com.
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2, as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
*/

#include "AudioCodec.h"

#include <algorithm>


void AudioCodecElement::SetupCodec()
{
	switch (audioFormat)
	{
	case AudioFormatEnum::Aac:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_AAC);
		break;

	case AudioFormatEnum::AacLatm:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_AAC_LATM);
		break;

	case AudioFormatEnum::Ac3:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_AC3);
		break;

	case AudioFormatEnum::Dts:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_DTS);
		break;

	case AudioFormatEnum::MpegLayer2:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_MP2);
		break;

	case AudioFormatEnum::Mpeg2Layer3:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_MP3);
		break;

	case AudioFormatEnum::WmaPro:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_WMAPRO);
		break;

	case AudioFormatEnum::DolbyTrueHD:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_TRUEHD);
		break;

	case AudioFormatEnum::EAc3:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_EAC3);
		break;

	case AudioFormatEnum::Opus:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_OPUS);
		break;

	case AudioFormatEnum::Vorbis:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_VORBIS);
		break;

	case AudioFormatEnum::PcmDvd:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_PCM_DVD);
		break;

	case AudioFormatEnum::Flac:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_FLAC);
		break;

	case AudioFormatEnum::PcmS16LE:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_PCM_S16LE);
		break;

	case AudioFormatEnum::PcmS24LE:
		soundCodec = avcodec_find_decoder(AV_CODEC_ID_PCM_S24LE);
		break;

	default:
		printf("Audio format %d is not supported.\n", (int)audioFormat);
		throw NotSupportedException();
	}

	if (!soundCodec)
	{
		throw Exception("codec not found\n");
	}


	soundCodecContext = avcodec_alloc_context3(soundCodec);
	if (!soundCodecContext)
	{
		throw Exception("avcodec_alloc_context3 failed.\n");
	}


	AudioPinInfoSPTR info = std::static_pointer_cast<AudioPinInfo>(audioInPin->Source()->Info());

	soundCodecContext->channels = alsa_channels;
	soundCodecContext->sample_rate = sampleRate;
	//soundCodecContext->sample_fmt = AV_SAMPLE_FMT_S16P; //AV_SAMPLE_FMT_FLTP; //AV_SAMPLE_FMT_S16P
	soundCodecContext->request_sample_fmt = AV_SAMPLE_FMT_FLTP; // AV_SAMPLE_FMT_S16P; //AV_SAMPLE_FMT_FLTP;
	soundCodecContext->request_channel_layout = AV_CH_LAYOUT_STEREO | AV_CH_FRONT_CENTER;

	if (info->ExtraData)
	{
		soundCodecContext->extradata_size = info->ExtraData->size();
		soundCodecContext->extradata = &info->ExtraData->operator[](0);
	}

	/* open it */
	if (avcodec_open2(soundCodecContext, soundCodec, NULL) < 0)
	{
		throw Exception("could not open codec\n");
	}
}


void AudioCodecElement::ProcessBuffer(AVPacketBufferPTR buffer, const AVFrameBufferUPTR& frame)
{
	AVPacket* pkt = buffer->GetAVPacket();
	AVFrame* decoded_frame = frame->GetAVFrame();


	// Decode audio
	//printf("Decoding frame (AVPacket=%p, size=%d).\n",
	//	buffer->GetAVPacket(), buffer->GetAVPacket()->size);

	int ret = 0;
	while (IsExecuting())
	{
		if (pkt != nullptr)
		{
			ret = avcodec_send_packet(soundCodecContext, pkt);
			pkt = nullptr;

			if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
			{
				break;
			}
		}

		ret = avcodec_receive_frame(soundCodecContext, decoded_frame);

		if (ret < 0)
		{
			break;
		}
		else
		{
			double pts = av_q2d(buffer->TimeBase()) * decoded_frame->pts;
			Log("decoded audio frame OK (pts=%f, pkt_size=%x)\n", pts, decoded_frame->pkt_size);


			// Convert audio to ALSA format
			//printf("decoded audio frame pts=%f\n", pts);

			// Copy out the PCM data because libav fills the frame
			// with re-used data pointers.
			PcmFormat format;
			bool isInterleaved;
			switch (decoded_frame->format)
			{
			case AV_SAMPLE_FMT_S16:
				format = PcmFormat::Int16;
				isInterleaved = true;
				break;

			case AV_SAMPLE_FMT_S32:
				format = PcmFormat::Int32;
				isInterleaved = true;
				break;

			case AV_SAMPLE_FMT_FLT:
				format = PcmFormat::Float32;
				isInterleaved = true;
				break;

			case AV_SAMPLE_FMT_S16P:
				format = PcmFormat::Int16Planes;
				isInterleaved = false;
				break;

			case AV_SAMPLE_FMT_S32P:
				format = PcmFormat::Int32Planes;
				isInterleaved = false;
				break;

			case AV_SAMPLE_FMT_FLTP:
				format = PcmFormat::Float32Planes;
				isInterleaved = false;
				break;

			default:
				printf("Sample format (%d) not supported.\n", decoded_frame->format);
				throw NotSupportedException();
			}


			PcmDataBufferUPTR pcmDataBuffer = std::make_unique<PcmDataBuffer>(
				shared_from_this(),
				format,
				decoded_frame->channels,
				decoded_frame->nb_samples);

			if (buffer->GetAVPacket()->pts != AV_NOPTS_VALUE)
			{
				pcmDataBuffer->SetTimeStamp(
					frame->GetAVFrame()->best_effort_timestamp *
					av_q2d(buffer->TimeBase()));
			}
			else
			{
				pcmDataBuffer->SetTimeStamp(-1);
			}


			if (isInterleaved)
			{
				PcmData* pcmData = pcmDataBuffer->GetPcmData();
				memcpy(pcmData->Channel[0], decoded_frame->data[0], pcmData->ChannelSize);
			}
			else
			{
				// left, right, center
				const int DOWNMIX_MAX_CHANNELS = 3;

				void* channels[DOWNMIX_MAX_CHANNELS] = { 0 };

				if (decoded_frame->channels == 1)
				{
					// Mono
					int monoChannelIndex = av_get_channel_layout_channel_index(
						decoded_frame->channel_layout,
						AV_CH_FRONT_CENTER);


					channels[0] = (void*)decoded_frame->data[monoChannelIndex];
				}
				else
				{
					int leftChannelIndex = av_get_channel_layout_channel_index(
						decoded_frame->channel_layout,
						AV_CH_FRONT_LEFT);

					if (leftChannelIndex < 0)
						throw InvalidOperationException("av_get_channel_layout_channel_index (AV_CH_FRONT_LEFT) failed.");


					int rightChannelIndex = av_get_channel_layout_channel_index(
						decoded_frame->channel_layout,
						AV_CH_FRONT_RIGHT);

					if (rightChannelIndex < 0)
						throw InvalidOperationException("av_get_channel_layout_channel_index (AV_CH_FRONT_RIGHT) failed.");


					int centerChannelIndex = av_get_channel_layout_channel_index(
						decoded_frame->channel_layout,
						AV_CH_FRONT_CENTER);


					channels[0] = (void*)decoded_frame->data[leftChannelIndex];
					channels[1] = (void*)decoded_frame->data[rightChannelIndex];

					if (decoded_frame->channels > 2)
					{
						if (centerChannelIndex < 0)
							throw InvalidOperationException("av_get_channel_layout_channel_index (AV_CH_FRONT_CENTER) failed.");

						channels[2] = (void*)decoded_frame->data[centerChannelIndex];
					}
					//else
					//{
					//	channels[2] = nullptr;
					//}
				}

				int channelCount = std::min(DOWNMIX_MAX_CHANNELS, decoded_frame->channels);
				PcmData* pcmData = pcmDataBuffer->GetPcmData();
				pcmData->Channels = channelCount;

				for (int i = 0; i < channelCount; ++i)
				{
					memcpy(pcmData->Channel[i], channels[i], pcmData->ChannelSize);
				}
			}

			audioOutPin->SendBuffer(std::move(pcmDataBuffer));
		}
	}

	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
	{
		// Report the error, but otherwise ignore it.
		char errmsg[AV_ERROR_MAX_STRING_SIZE] = { 0 };
		av_strerror(ret, errmsg, AV_ERROR_MAX_STRING_SIZE);

		Log("Error while decoding audio: %s\n", errmsg);
	}
}



void AudioCodecElement::Initialize()
{
	ClearOutputPins();
	ClearInputPins();

	// TODO: Pin format negotiation

	{
		// Create an audio in pin
		AudioPinInfoSPTR info = std::make_shared<AudioPinInfo>();
		info->Format = AudioFormatEnum::Unknown;
		info->Channels = 0;
		info->SampleRate = 0;

		ElementWPTR weakPtr = shared_from_this();
		audioInPin = std::make_shared<InPin>(weakPtr, info);
		AddInputPin(audioInPin);
	}

	{
		// Create an audio out pin
		outInfo = std::make_shared<AudioPinInfo>();
		outInfo->Format = AudioFormatEnum::Pcm;
		outInfo->Channels = 0;
		outInfo->SampleRate = 0;

		ElementWPTR weakPtr = shared_from_this();
		audioOutPin = std::make_shared<OutPin>(weakPtr, outInfo);
		AddOutputPin(audioOutPin);
	}


	// Create buffer(s)
	frame = std::make_unique<AVFrameBuffer>(shared_from_this());
}

void AudioCodecElement::DoWork()
{
	BufferUPTR buffer;

	// Reap output buffers
	while (audioOutPin->TryGetAvailableBuffer(&buffer))
	{
		// New buffers are created as needed so just
		// drop this buffer.
		Wake();
	}


	if (audioInPin->TryGetFilledBuffer(&buffer))
	{
		if (buffer->Type() != BufferTypeEnum::AVPacket)
		{
			// The input buffer is not usuable for processing
			switch (buffer->Type())
			{
			case BufferTypeEnum::Marker:
			{
				MarkerBufferPTR markerBuffer = static_cast<MarkerBufferPTR>(buffer.get());

				switch (markerBuffer->Marker())
				{
				case MarkerEnum::EndOfStream:
					// Send all Output Pins an EOS buffer					
					for (int i = 0; i < Outputs()->Count(); ++i)
					{
						MarkerBufferUPTR eosBuffer = std::make_unique<MarkerBuffer>(shared_from_this(), MarkerEnum::EndOfStream);
						Outputs()->Item(i)->SendBuffer(std::move(eosBuffer));
					}

					//SetExecutionState(ExecutionStateEnum::Idle);
					SetState(MediaState::Pause);
					break;

				default:
					// ignore unknown 
					break;
				}
				break;
			}

			default:
				// Ignore
				break;
			}

			audioInPin->PushProcessedBuffer(std::move(buffer));
			audioInPin->ReturnProcessedBuffers();
		}
		else
		{
			if (isFirstData)
			{
				OutPinSPTR otherPin = audioInPin->Source();
				if (otherPin)
				{
					if (otherPin->Info()->Category() != MediaCategoryEnum::Audio)
					{
						throw InvalidOperationException("AlsaAudioSinkElement: Not connected to an audio pin.");
					}

					AudioPinInfoSPTR info = std::static_pointer_cast<AudioPinInfo>(otherPin->Info());
					audioFormat = info->Format;
					sampleRate = info->SampleRate;
					streamChannels = info->Channels;

					// TODO: Copy in pin info


					outInfo->SampleRate = info->SampleRate;
					outInfo->Channels = info->Channels;

					printf("AudioCodecElement: outInfo->SampleRate=%d, outInfo->Channels=%d\n", outInfo->SampleRate, outInfo->Channels);

					SetupCodec();

					isFirstData = false;
				}
			}

			AVPacketBufferPTR avPacketBuffer = static_cast<AVPacketBufferPTR>(buffer.get());
			ProcessBuffer(avPacketBuffer, frame);

			audioInPin->PushProcessedBuffer(std::move(buffer));
			audioInPin->ReturnProcessedBuffers();
		}
	}
}

void AudioCodecElement::ChangeState(MediaState oldState, MediaState newState)
{
	Element::ChangeState(oldState, newState);
}

void AudioCodecElement::Flush()
{
	Element::Flush();

	if (soundCodecContext)
	{
		avcodec_flush_buffers(soundCodecContext);
	}
}
