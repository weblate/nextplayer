/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <android/log.h>
#include <jni.h>
#include <cstdlib>

extern "C" {
#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <cstdint>
#endif
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#define LOG_TAG "ffmpeg_jni"
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

#define ERROR_STRING_BUFFER_LENGTH 256

// Output format corresponding to AudioFormat.ENCODING_PCM_16BIT.
static const AVSampleFormat OUTPUT_FORMAT_PCM_16BIT = AV_SAMPLE_FMT_S16;
// Output format corresponding to AudioFormat.ENCODING_PCM_FLOAT.
static const AVSampleFormat OUTPUT_FORMAT_PCM_FLOAT = AV_SAMPLE_FMT_FLT;

static const int AUDIO_DECODER_ERROR_INVALID_DATA = -1;
static const int AUDIO_DECODER_ERROR_OTHER = -2;

/**
 * Returns the AVCodec with the specified name, or NULL if it is not available.
 */
AVCodec *getCodecByName(JNIEnv *env, jstring codecName);

/**
 * Allocates and opens a new AVCodecContext for the specified codec, passing the
 * provided extraData as initialization data for the decoder if it is non-NULL.
 * Returns the created context.
 */
AVCodecContext *createContext(JNIEnv *env, AVCodec *codec, jbyteArray extraData,
                              jboolean outputFloat, jint rawSampleRate,
                              jint rawChannelCount);

/**
 * Decodes the packet into the output buffer, returning the number of bytes
 * written, or a negative AUDIO_DECODER_ERROR constant value in the case of an
 * error.
 */
int decodePacket(AVCodecContext *context, AVPacket *packet,
                 uint8_t *outputBuffer, int outputSize);

/**
 * Transforms ffmpeg AVERROR into a negative AUDIO_DECODER_ERROR constant value.
 */
int transformError(int errorNumber);

/**
 * Outputs a log message describing the avcodec error number.
 */
void logError(const char *functionName, int errorNumber);

/**
 * Releases the specified context.
 */
void releaseContext(AVCodecContext *context);

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }
    return JNI_VERSION_1_6;
}

AVCodec *getCodecByName(JNIEnv *env, jstring codecName) {
    if (!codecName) {
        return nullptr;
    }
    const char *codecNameChars = env->GetStringUTFChars(codecName, nullptr);
    auto *codec = const_cast<AVCodec *>(avcodec_find_decoder_by_name(codecNameChars));
    env->ReleaseStringUTFChars(codecName, codecNameChars);
    return codec;
}

AVCodecContext *createContext(JNIEnv *env, AVCodec *codec, jbyteArray extraData,
                              jboolean outputFloat, jint rawSampleRate,
                              jint rawChannelCount) {
    AVCodecContext *context = avcodec_alloc_context3(codec);
    if (!context) {
        LOGE("Failed to allocate context.");
        return nullptr;
    }
    context->request_sample_fmt =
            outputFloat ? OUTPUT_FORMAT_PCM_FLOAT : OUTPUT_FORMAT_PCM_16BIT;
    if (extraData) {
        jsize size = env->GetArrayLength(extraData);
        context->extradata_size = size;
        context->extradata =
                (uint8_t *)av_malloc(size + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!context->extradata) {
            LOGE("Failed to allocate extra data.");
            releaseContext(context);
            return nullptr;
        }
        env->GetByteArrayRegion(extraData, 0, size, (jbyte *)context->extradata);
    }
    if (context->codec_id == AV_CODEC_ID_PCM_MULAW ||
        context->codec_id == AV_CODEC_ID_PCM_ALAW) {
        context->sample_rate = rawSampleRate;
        context->ch_layout.nb_channels = rawChannelCount;
        av_channel_layout_default(&context->ch_layout, rawChannelCount);
    }
    context->err_recognition = AV_EF_IGNORE_ERR;
    int result = avcodec_open2(context, codec, nullptr);
    if (result < 0) {
        logError("avcodec_open2", result);
        releaseContext(context);
        return nullptr;
    }
    return context;
}

int decodePacket(AVCodecContext *context, AVPacket *packet,
                 uint8_t *outputBuffer, int outputSize) {
    int result = 0;
    // Queue input data.
    result = avcodec_send_packet(context, packet);
    if (result) {
        logError("avcodec_send_packet", result);
        return transformError(result);
    }

    // Dequeue output data until it runs out.
    int outSize = 0;
    while (true) {
        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            LOGE("Failed to allocate output frame.");
            return AUDIO_DECODER_ERROR_INVALID_DATA;
        }
        result = avcodec_receive_frame(context, frame);
        if (result) {
            av_frame_free(&frame);
            if (result == AVERROR(EAGAIN)) {
                break;
            }
            logError("avcodec_receive_frame", result);
            return transformError(result);
        }

        // Resample output.
        AVSampleFormat sampleFormat = context->sample_fmt;
        int channelCount = context->ch_layout.nb_channels;
        int channelLayout = (int)context->ch_layout.u.mask;
        int sampleRate = context->sample_rate;
        int sampleCount = frame->nb_samples;
        int dataSize = av_samples_get_buffer_size(nullptr, channelCount, sampleCount,
                                                  sampleFormat, 1);
        SwrContext *resampleContext;
        if (context->opaque) {
            resampleContext = (SwrContext *)context->opaque;
        } else {
            resampleContext = swr_alloc();
            av_opt_set_int(resampleContext, "in_channel_layout", channelLayout, 0);
            av_opt_set_int(resampleContext, "out_channel_layout", channelLayout, 0);
            av_opt_set_int(resampleContext, "in_sample_rate", sampleRate, 0);
            av_opt_set_int(resampleContext, "out_sample_rate", sampleRate, 0);
            av_opt_set_int(resampleContext, "in_sample_fmt", sampleFormat, 0);
            // The output format is always the requested format.
            av_opt_set_int(resampleContext, "out_sample_fmt",
                           context->request_sample_fmt, 0);
            result = swr_init(resampleContext);
            if (result < 0) {
                logError("swr_init", result);
                av_frame_free(&frame);
                return transformError(result);
            }
            context->opaque = resampleContext;
        }
        int inSampleSize = av_get_bytes_per_sample(sampleFormat);
        int outSampleSize = av_get_bytes_per_sample(context->request_sample_fmt);
        int outSamples = swr_get_out_samples(resampleContext, sampleCount);
        int bufferOutSize = outSampleSize * channelCount * outSamples;
        if (outSize + bufferOutSize > outputSize) {
            LOGE("Output buffer size (%d) too small for output data (%d).",
                 outputSize, outSize + bufferOutSize);
            av_frame_free(&frame);
            return AUDIO_DECODER_ERROR_INVALID_DATA;
        }
        result = swr_convert(resampleContext, &outputBuffer, bufferOutSize,
                             (const uint8_t **)frame->data, frame->nb_samples);
        av_frame_free(&frame);
        if (result < 0) {
            logError("swr_convert", result);
            return AUDIO_DECODER_ERROR_INVALID_DATA;
        }
        int available = swr_get_out_samples(resampleContext, 0);
        if (available != 0) {
            LOGE("Expected no samples remaining after resampling, but found %d.",
                 available);
            return AUDIO_DECODER_ERROR_INVALID_DATA;
        }
        outputBuffer += bufferOutSize;
        outSize += bufferOutSize;
    }
    return outSize;
}

int transformError(int errorNumber) {
    return errorNumber == AVERROR_INVALIDDATA ? AUDIO_DECODER_ERROR_INVALID_DATA
                                              : AUDIO_DECODER_ERROR_OTHER;
}

void logError(const char *functionName, int errorNumber) {
    char *buffer = (char *)malloc(ERROR_STRING_BUFFER_LENGTH * sizeof(char));
    av_strerror(errorNumber, buffer, ERROR_STRING_BUFFER_LENGTH);
    LOGE("Error in %s: %s", functionName, buffer);
    free(buffer);
}

void releaseContext(AVCodecContext *context) {
    if (!context) {
        return;
    }
    SwrContext *swrContext;
    if ((swrContext = (SwrContext *)context->opaque)) {
        swr_free(&swrContext);
        context->opaque = nullptr;
    }
    avcodec_free_context(&context);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_dev_anilbeesetti_libs_ffcodecs_FfmpegAudioDecoder_ffmpegInitialize(JNIEnv *env,
                                                                                    jobject thiz,
                                                                                    jstring codec_name,
                                                                                    jbyteArray extra_data,
                                                                                    jboolean output_float,
                                                                                    jint raw_sample_rate,
                                                                                    jint raw_channel_count) {
    AVCodec *codec = getCodecByName(env, codec_name);
    if (!codec) {
        LOGE("Codec not found.");
        return 0L;
    }
    return (jlong)createContext(env, codec, extra_data, output_float, raw_sample_rate,
                                raw_channel_count);
}
extern "C"
JNIEXPORT jint JNICALL
Java_dev_anilbeesetti_libs_ffcodecs_FfmpegAudioDecoder_ffmpegDecode(JNIEnv *env,
                                                                                jobject thiz,
                                                                                jlong context,
                                                                                jobject input_data,
                                                                                jint input_size,
                                                                                jobject output_data,
                                                                                jint output_size) {
    if (!context) {
        LOGE("Context must be non-NULL.");
        return -1;
    }
    if (!input_data || !output_data) {
        LOGE("Input and output buffers must be non-NULL.");
        return -1;
    }
    if (input_size < 0) {
        LOGE("Invalid input buffer size: %d.", input_size);
        return -1;
    }
    if (output_size < 0) {
        LOGE("Invalid output buffer length: %d", output_size);
        return -1;
    }
    auto *inputBuffer = (uint8_t *)env->GetDirectBufferAddress(input_data);
    auto *outputBuffer = (uint8_t *)env->GetDirectBufferAddress(output_data);
    AVPacket *packet;
    packet = av_packet_alloc();

    if (packet == nullptr) {
        LOGE("audio_decoder_decode_frame: av_packet_alloc failed");
        return -1;
    }

    packet->data = inputBuffer;
    packet->size = input_size;
    int decodedPacket = decodePacket((AVCodecContext *)context, packet, outputBuffer,
                        output_size);
    av_packet_free(&packet);
    return decodedPacket;
}
extern "C"
JNIEXPORT jint JNICALL
Java_dev_anilbeesetti_libs_ffcodecs_FfmpegAudioDecoder_ffmpegGetChannelCount(
        JNIEnv *env, jobject thiz, jlong context) {
    if (!context) {
        LOGE("Context must be non-NULL.");
        return -1;
    }
    return ((AVCodecContext *)context)->ch_layout.nb_channels;
}
extern "C"
JNIEXPORT jint JNICALL
Java_dev_anilbeesetti_libs_ffcodecs_FfmpegAudioDecoder_ffmpegGetSampleRate(JNIEnv *env,
                                                                                       jobject thiz,
                                                                                       jlong context) {
    if (!context) {
        LOGE("Context must be non-NULL.");
        return -1;
    }
    return ((AVCodecContext *)context)->sample_rate;
}
extern "C"
JNIEXPORT jlong JNICALL
Java_dev_anilbeesetti_libs_ffcodecs_FfmpegAudioDecoder_ffmpegReset(JNIEnv *env,
                                                                               jobject thiz,
                                                                               jlong jContext,
                                                                               jbyteArray extra_data) {
    auto *context = (AVCodecContext *)jContext;
    if (!context) {
        LOGE("Tried to reset without a context.");
        return 0L;
    }

    AVCodecID codecId = context->codec_id;
    if (codecId == AV_CODEC_ID_TRUEHD) {
        // Release and recreate the context if the codec is TrueHD.
        // TODO: Figure out why flushing doesn't work for this codec.
        releaseContext(context);
        auto *codec = const_cast<AVCodec *>(avcodec_find_decoder(codecId));
        if (!codec) {
            LOGE("Unexpected error finding codec %d.", codecId);
            return 0L;
        }
        auto outputFloat =
                (jboolean)(context->request_sample_fmt == OUTPUT_FORMAT_PCM_FLOAT);
        return (jlong)createContext(env, codec, extra_data, outputFloat,
                /* rawSampleRate= */ -1,
                /* rawChannelCount= */ -1);
    }

    avcodec_flush_buffers(context);
    return (jlong)context;
}
extern "C"
JNIEXPORT void JNICALL
Java_dev_anilbeesetti_libs_ffcodecs_FfmpegAudioDecoder_ffmpegRelease(JNIEnv *env,
                                                                                 jobject thiz,
                                                                                 jlong context) {
    if (context) {
        releaseContext((AVCodecContext *)context);
    }
}
extern "C"
JNIEXPORT jstring JNICALL
Java_dev_anilbeesetti_libs_ffcodecs_FfmpegLibrary_ffmpegGetVersion(JNIEnv *env,
                                                                               jclass clazz) {
    return env->NewStringUTF(LIBAVCODEC_IDENT);
}
extern "C"
JNIEXPORT jint JNICALL
Java_dev_anilbeesetti_libs_ffcodecs_FfmpegLibrary_ffmpegGetInputBufferPaddingSize(
        JNIEnv *env, jclass clazz) {
    return (jint)AV_INPUT_BUFFER_PADDING_SIZE;
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_dev_anilbeesetti_libs_ffcodecs_FfmpegLibrary_ffmpegHasDecoder(JNIEnv *env,
                                                                               jclass clazz,
                                                                               jstring codec_name) {
    return getCodecByName(env, codec_name) != nullptr;
}