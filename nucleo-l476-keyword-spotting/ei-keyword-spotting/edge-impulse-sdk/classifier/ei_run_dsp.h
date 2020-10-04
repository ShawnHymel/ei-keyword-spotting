/* Edge Impulse inferencing library
 * Copyright (c) 2020 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _EDGE_IMPULSE_RUN_DSP_H_
#define _EDGE_IMPULSE_RUN_DSP_H_

#include "../../../ei-keyword-spotting/edge-impulse-sdk/dsp/spectral/spectral.hpp"
#include "../../../ei-keyword-spotting/edge-impulse-sdk/dsp/speechpy/speechpy.hpp"
#include "../../../ei-keyword-spotting/model-parameters/model_metadata.h"

#if defined(__cplusplus) && EI_C_LINKAGE == 1
extern "C" {
    extern void ei_printf(const char *format, ...);
}
#else
extern void ei_printf(const char *format, ...);
#endif

#ifdef __cplusplus
namespace {
#endif // __cplusplus

using namespace ei;

__attribute__((unused)) int extract_spectral_analysis_features(signal_t *signal, matrix_t *output_matrix, void *config_ptr) {
    ei_dsp_config_spectral_analysis_t config = *((ei_dsp_config_spectral_analysis_t*)config_ptr);

    int ret;

    const float sampling_freq = EI_CLASSIFIER_FREQUENCY;

    // input matrix from the raw signal
    matrix_t input_matrix(signal->total_length / config.axes, config.axes);
    if (!input_matrix.buffer) {
        EIDSP_ERR(EIDSP_OUT_OF_MEM);
    }
    signal->get_data(0, signal->total_length, input_matrix.buffer);

    // scale the signal
    ret = numpy::scale(&input_matrix, config.scale_axes);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: Failed to scale signal (%d)\n", ret);
        EIDSP_ERR(ret);
    }

    // transpose the matrix so we have one row per axis (nifty!)
    ret = numpy::transpose(&input_matrix);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: Failed to transpose matrix (%d)\n", ret);
        EIDSP_ERR(ret);
    }

    // the spectral edges that we want to calculate
    matrix_t edges_matrix_in(64, 1);
    size_t edge_matrix_ix = 0;

    char spectral_str[128] = { 0 };
    if (strlen(config.spectral_power_edges) > sizeof(spectral_str) - 1) {
        EIDSP_ERR(EIDSP_PARAMETER_INVALID);
    }
    memcpy(spectral_str, config.spectral_power_edges, strlen(config.spectral_power_edges));

	char spectral_delim[] = ",";
	char *spectral_ptr = strtok(spectral_str, spectral_delim);
	while (spectral_ptr != NULL) {
        edges_matrix_in.buffer[edge_matrix_ix] = atof(spectral_ptr);
        edge_matrix_ix++;
		spectral_ptr = strtok(NULL, spectral_delim);
	}
    edges_matrix_in.rows = edge_matrix_ix;

    // calculate how much room we need for the output matrix
    size_t output_matrix_cols = spectral::feature::calculate_spectral_buffer_size(
        true, config.spectral_peaks_count, edges_matrix_in.rows
    );
    // ei_printf("output_matrix_size %hux%zu\n", input_matrix.rows, output_matrix_cols);
    if (output_matrix->cols * output_matrix->rows != static_cast<uint32_t>(output_matrix_cols * config.axes)) {
        EIDSP_ERR(EIDSP_MATRIX_SIZE_MISMATCH);
    }

    output_matrix->cols = output_matrix_cols;
    output_matrix->rows = config.axes;

    spectral::filter_t filter_type;
    if (strcmp(config.filter_type, "low") == 0) {
        filter_type = spectral::filter_lowpass;
    }
    else if (strcmp(config.filter_type, "high") == 0) {
        filter_type = spectral::filter_highpass;
    }
    else {
        filter_type = spectral::filter_none;
    }

    ret = spectral::feature::spectral_analysis(output_matrix, &input_matrix,
        sampling_freq, filter_type, config.filter_cutoff, config.filter_order,
        config.fft_length, config.spectral_peaks_count, config.spectral_peaks_threshold, &edges_matrix_in);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: Failed to calculate spectral features (%d)\n", ret);
        EIDSP_ERR(ret);
    }

    // flatten again
    output_matrix->cols = config.axes * output_matrix_cols;
    output_matrix->rows = 1;

    return EIDSP_OK;
}

__attribute__((unused)) int extract_raw_features(signal_t *signal, matrix_t *output_matrix, void *config_ptr) {
    ei_dsp_config_raw_t config = *((ei_dsp_config_raw_t*)config_ptr);

    // input matrix from the raw signal
    matrix_t input_matrix(signal->total_length / config.axes, config.axes);
    if (!input_matrix.buffer) {
        EIDSP_ERR(EIDSP_OUT_OF_MEM);
    }
    signal->get_data(0, signal->total_length, input_matrix.buffer);

    // scale the signal
    int ret = numpy::scale(&input_matrix, config.scale_axes);
    if (ret != EIDSP_OK) {
        EIDSP_ERR(ret);
    }

    memcpy(output_matrix->buffer, input_matrix.buffer, signal->total_length * sizeof(float));

    return EIDSP_OK;
}

__attribute__((unused)) int extract_flatten_features(signal_t *signal, matrix_t *output_matrix, void *config_ptr) {
    ei_dsp_config_flatten_t config = *((ei_dsp_config_flatten_t*)config_ptr);

    uint32_t expected_matrix_size = 0;
    if (config.average) expected_matrix_size += config.axes;
    if (config.minimum) expected_matrix_size += config.axes;
    if (config.maximum) expected_matrix_size += config.axes;
    if (config.rms) expected_matrix_size += config.axes;
    if (config.stdev) expected_matrix_size += config.axes;
    if (config.skewness) expected_matrix_size += config.axes;
    if (config.kurtosis) expected_matrix_size += config.axes;

    if (output_matrix->rows * output_matrix->cols != expected_matrix_size) {
        EIDSP_ERR(EIDSP_MATRIX_SIZE_MISMATCH);
    }

    int ret;

    // input matrix from the raw signal
    matrix_t input_matrix(signal->total_length / config.axes, config.axes);
    if (!input_matrix.buffer) {
        EIDSP_ERR(EIDSP_OUT_OF_MEM);
    }
    signal->get_data(0, signal->total_length, input_matrix.buffer);

    // scale the signal
    ret = numpy::scale(&input_matrix, config.scale_axes);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: Failed to scale signal (%d)\n", ret);
        EIDSP_ERR(ret);
    }

    // transpose the matrix so we have one row per axis (nifty!)
    ret = numpy::transpose(&input_matrix);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: Failed to transpose matrix (%d)\n", ret);
        EIDSP_ERR(ret);
    }

    size_t out_matrix_ix = 0;

    for (size_t row = 0; row < input_matrix.rows; row++) {
        matrix_t row_matrix(1, input_matrix.cols, input_matrix.buffer + (row * input_matrix.cols));

        if (config.average) {
            float fbuffer;
            matrix_t out_matrix(1, 1, &fbuffer);
            numpy::mean(&row_matrix, &out_matrix);
            output_matrix->buffer[out_matrix_ix++] = out_matrix.buffer[0];
        }

        if (config.minimum) {
            float fbuffer;
            matrix_t out_matrix(1, 1, &fbuffer);
            numpy::min(&row_matrix, &out_matrix);
            output_matrix->buffer[out_matrix_ix++] = out_matrix.buffer[0];
        }

        if (config.maximum) {
            float fbuffer;
            matrix_t out_matrix(1, 1, &fbuffer);
            numpy::max(&row_matrix, &out_matrix);
            output_matrix->buffer[out_matrix_ix++] = out_matrix.buffer[0];
        }

        if (config.rms) {
            float fbuffer;
            matrix_t out_matrix(1, 1, &fbuffer);
            numpy::rms(&row_matrix, &out_matrix);
            output_matrix->buffer[out_matrix_ix++] = out_matrix.buffer[0];
        }

        if (config.stdev) {
            float fbuffer;
            matrix_t out_matrix(1, 1, &fbuffer);
            numpy::stdev(&row_matrix, &out_matrix);
            output_matrix->buffer[out_matrix_ix++] = out_matrix.buffer[0];
        }

        if (config.skewness) {
            float fbuffer;
            matrix_t out_matrix(1, 1, &fbuffer);
            numpy::skew(&row_matrix, &out_matrix);
            output_matrix->buffer[out_matrix_ix++] = out_matrix.buffer[0];
        }

        if (config.kurtosis) {
            float fbuffer;
            matrix_t out_matrix(1, 1, &fbuffer);
            numpy::kurtosis(&row_matrix, &out_matrix);
            output_matrix->buffer[out_matrix_ix++] = out_matrix.buffer[0];
        }
    }

    // flatten again
    output_matrix->cols = output_matrix->rows * output_matrix->cols;
    output_matrix->rows = 1;

    return EIDSP_OK;
}

static class speechpy::processing::preemphasis *preemphasis;
static int preemphasized_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    return preemphasis->get_data(offset, length, out_ptr);
}

__attribute__((unused)) int extract_mfcc_features(signal_t *signal, matrix_t *output_matrix, void *config_ptr) {
    ei_dsp_config_mfcc_t config = *((ei_dsp_config_mfcc_t*)config_ptr);

    if (config.axes != 1) {
        EIDSP_ERR(EIDSP_MATRIX_SIZE_MISMATCH);
    }

    // @todo: move this to config
    const uint32_t frequency = static_cast<uint32_t>(EI_CLASSIFIER_FREQUENCY);

    // preemphasis class to preprocess the audio...
    class speechpy::processing::preemphasis pre(signal, config.pre_shift, config.pre_cof);
    preemphasis = &pre;

    signal_t preemphasized_audio_signal;
    preemphasized_audio_signal.total_length = signal->total_length;
    preemphasized_audio_signal.get_data = &preemphasized_audio_signal_get_data;

    // calculate the size of the MFCC matrix
    matrix_size_t out_matrix_size =
        speechpy::feature::calculate_mfcc_buffer_size(
            signal->total_length, frequency, config.frame_length, config.frame_stride, config.num_cepstral);
    /* Only throw size mismatch error calculated buffer doesn't fit for continuous inferencing */
    if (out_matrix_size.rows * out_matrix_size.cols > output_matrix->rows * output_matrix->cols) {
        ei_printf("out_matrix = %hux%hu\n", output_matrix->rows, output_matrix->cols);
        ei_printf("calculated size = %hux%hu\n", out_matrix_size.rows, out_matrix_size.cols);
        EIDSP_ERR(EIDSP_MATRIX_SIZE_MISMATCH);
    }

    output_matrix->rows = out_matrix_size.rows;
    output_matrix->cols = out_matrix_size.cols;

    // and run the MFCC extraction (using 32 rather than 40 filters here to optimize speed on embedded)
    int ret = speechpy::feature::mfcc(output_matrix, &preemphasized_audio_signal,
        frequency, config.frame_length, config.frame_stride, config.num_cepstral, config.num_filters, config.fft_length,
        config.low_frequency, config.high_frequency);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: MFCC failed (%d)\n", ret);
        EIDSP_ERR(ret);
    }

    // cepstral mean and variance normalization
    ret = speechpy::processing::cmvnw(output_matrix, config.win_size, true);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: cmvnw failed (%d)\n", ret);
        EIDSP_ERR(ret);
    }

    output_matrix->cols = out_matrix_size.rows * out_matrix_size.cols;
    output_matrix->rows = 1;

    return EIDSP_OK;
}

__attribute__((unused)) int extract_mfcc_per_slice_features(signal_t *signal, matrix_t *output_matrix, void *config_ptr) {
    ei_dsp_config_mfcc_t config = *((ei_dsp_config_mfcc_t*)config_ptr);

    static bool first_run = false;

    if (config.axes != 1) {
        EIDSP_ERR(EIDSP_MATRIX_SIZE_MISMATCH);
    }

    /* Fake an extra frame_length for stack frames calculations. There, 1 frame_length is always
    subtracted and there for never used. But skip the first slice to fit the feature_matrix
    buffer */
    if (first_run == true) {
        signal->total_length += (size_t)(config.frame_length * (float)EI_CLASSIFIER_FREQUENCY);
    }

    first_run = true;

    // @todo: move this to config
    const uint32_t frequency = static_cast<uint32_t>(EI_CLASSIFIER_FREQUENCY);

    // preemphasis class to preprocess the audio...
    class speechpy::processing::preemphasis pre(signal, config.pre_shift, config.pre_cof);
    preemphasis = &pre;

    signal_t preemphasized_audio_signal;
    preemphasized_audio_signal.total_length = signal->total_length;
    preemphasized_audio_signal.get_data = &preemphasized_audio_signal_get_data;

    // calculate the size of the MFCC matrix
    matrix_size_t out_matrix_size =
        speechpy::feature::calculate_mfcc_buffer_size(
            signal->total_length, frequency, config.frame_length, config.frame_stride, config.num_cepstral);
    /* Only throw size mismatch error calculated buffer doesn't fit for continuous inferencing */
    if (out_matrix_size.rows * out_matrix_size.cols > output_matrix->rows * output_matrix->cols) {
        ei_printf("out_matrix = %hux%hu\n", output_matrix->rows, output_matrix->cols);
        ei_printf("calculated size = %hux%hu\n", out_matrix_size.rows, out_matrix_size.cols);
        EIDSP_ERR(EIDSP_MATRIX_SIZE_MISMATCH);
    }

    output_matrix->rows = out_matrix_size.rows;
    output_matrix->cols = out_matrix_size.cols;

    // and run the MFCC extraction (using 32 rather than 40 filters here to optimize speed on embedded)
    int ret = speechpy::feature::mfcc(output_matrix, &preemphasized_audio_signal,
        frequency, config.frame_length, config.frame_stride, config.num_cepstral, config.num_filters, config.fft_length,
        config.low_frequency, config.high_frequency);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: MFCC failed (%d)\n", ret);
        EIDSP_ERR(ret);
    }

    output_matrix->cols = out_matrix_size.rows * out_matrix_size.cols;
    output_matrix->rows = 1;

    return EIDSP_OK;
}

__attribute__((unused)) int extract_image_features(signal_t *signal, matrix_t *output_matrix, void *config_ptr) {
    ei_dsp_config_image_t config = *((ei_dsp_config_image_t*)config_ptr);

    int16_t channel_count = strcmp(config.channels, "Grayscale") == 0 ? 1 : 3;

    if (output_matrix->rows * output_matrix->cols != EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT * channel_count) {
        ei_printf("out_matrix = %hu items\n", output_matrix->rows, output_matrix->cols);
        ei_printf("calculated size = %hu items\n", EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT * channel_count);
        EIDSP_ERR(EIDSP_MATRIX_SIZE_MISMATCH);
    }

    size_t output_ix = 0;

    // buffered read from the signal
    size_t bytes_left = signal->total_length;
    for (size_t ix = 0; ix < signal->total_length; ix += 4096) {
        size_t bytes_to_read = bytes_left > 4096 ? 4096 : bytes_left;

        matrix_t input_matrix(bytes_to_read, config.axes);
        if (!input_matrix.buffer) {
            EIDSP_ERR(EIDSP_OUT_OF_MEM);
        }
        signal->get_data(ix, bytes_to_read, input_matrix.buffer);

        for (size_t jx = 0; jx < bytes_to_read; jx++) {
            uint32_t pixel = static_cast<uint32_t>(input_matrix.buffer[jx]);
            if (channel_count == 3) {
                // rgb to 0..1
                output_matrix->buffer[output_ix++] = static_cast<float>(pixel >> 16 & 0xff) / 255.0f;
                output_matrix->buffer[output_ix++] = static_cast<float>(pixel >> 8 & 0xff) / 255.0f;
                output_matrix->buffer[output_ix++] = static_cast<float>(pixel & 0xff) / 255.0f;
            }
            else {
                // grayscale conversion (also to 0..1)
                float r = static_cast<float>(pixel >> 16 & 0xff) / 255.0f;
                float g = static_cast<float>(pixel >> 8 & 0xff) / 255.0f;
                float b = static_cast<float>(pixel & 0xff) / 255.0f;
                // ITU-R 601-2 luma transform
                // see: https://pillow.readthedocs.io/en/stable/reference/Image.html#PIL.Image.Image.convert
                output_matrix->buffer[output_ix++] = (0.299f * r) + (0.587f * g) + (0.114f * b);
            }
        }

        bytes_left -= bytes_to_read;
    }

    return EIDSP_OK;
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _EDGE_IMPULSE_RUN_DSP_H_
