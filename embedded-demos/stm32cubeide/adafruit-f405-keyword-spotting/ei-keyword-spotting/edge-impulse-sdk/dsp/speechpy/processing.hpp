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

#ifndef _EIDSP_SPEECHPY_PROCESSING_H_
#define _EIDSP_SPEECHPY_PROCESSING_H_

#include "../../../../ei-keyword-spotting/edge-impulse-sdk/dsp/numpy.hpp"

namespace ei {
namespace speechpy {

// one stack frame returned by stack_frames
typedef struct ei_stack_frames_info {
    signal_t *signal;
    std::vector<uint32_t> *frame_ixs;
    int frame_length;

    // start_ixs is owned by us
    ~ei_stack_frames_info() {
        if (frame_ixs) {
            delete frame_ixs;
        }
    }
} stack_frames_info_t;

namespace processing {
    /**
     * Lazy Preemphasising on the signal.
     * @param signal: The input signal.
     * @param shift (int): The shift step.
     * @param cof (float): The preemphasising coefficient. 0 equals to no filtering.
     */
    class preemphasis {
public:
        preemphasis(ei_signal_t *signal, int shift = 1, float cof = 0.98f)
            : _signal(signal), _shift(shift), _cof(cof)
        {
            _prev_buffer = (float*)ei_dsp_calloc(shift * sizeof(float), 1);
            _end_of_signal_buffer = (float*)ei_dsp_calloc(shift * sizeof(float), 1);
            _next_offset_should_be = 0;

            if (shift < 0) {
                _shift = signal->total_length + shift;
            }

            if (!_prev_buffer || !_end_of_signal_buffer) return;

            // we need to get the shift bytes from the end of the buffer...
            signal->get_data(signal->total_length - shift, shift, _end_of_signal_buffer);
        }

        /**
         * Get preemphasized data from the underlying audio buffer...
         * This retrieves data from the signal then preemphasizes it.
         * @param offset Offset in the audio signal
         * @param length Length of the audio signal
         */
        int get_data(size_t offset, size_t length, float *out_buffer) {
            if (!_prev_buffer || !_end_of_signal_buffer) {
                EIDSP_ERR(EIDSP_OUT_OF_MEM);
            }
            if (offset + length > _signal->total_length) {
                EIDSP_ERR(EIDSP_OUT_OF_BOUNDS);
            }

            int ret;
            if (static_cast<int32_t>(offset) - _shift >= 0) {
                ret = _signal->get_data(offset - _shift, _shift, _prev_buffer);
                if (ret != 0) {
                    EIDSP_ERR(ret);
                }
            }
            // else we'll use the end_of_signal_buffer; so no need to check

            ret = _signal->get_data(offset, length, out_buffer);
            if (ret != 0) {
                EIDSP_ERR(ret);
            }

            // now we have the signal and we can preemphasize
            for (size_t ix = 0; ix < length; ix++) {
                float now = out_buffer[ix];

                // under shift? read from end
                if (offset + ix < static_cast<uint32_t>(_shift)) {
                    out_buffer[ix] = now - (_cof * _end_of_signal_buffer[offset + ix]);
                }
                // otherwise read from history buffer
                else {
                    out_buffer[ix] = now - (_cof * _prev_buffer[0]);
                }

                // roll through and overwrite last element
                numpy::roll(_prev_buffer, _shift, -1);
                _prev_buffer[_shift - 1] = now;
            }

            _next_offset_should_be += length;

            return EIDSP_OK;
        }

        ~preemphasis() {
            if (_prev_buffer) {
                ei_dsp_free(_prev_buffer, _shift * sizeof(float));
            }
            if (_end_of_signal_buffer) {
                ei_dsp_free(_end_of_signal_buffer, _shift * sizeof(float));
            }
        }

private:
        ei_signal_t *_signal;
        int _shift;
        float _cof;
        float *_prev_buffer;
        float *_end_of_signal_buffer;
        size_t _next_offset_should_be;
    };
}

namespace processing {
    /**
     * Preemphasising on the signal. This modifies the signal in place!
     * For memory consumption reasons you **probably** want the preemphasis class,
     * which lazily loads the signal in.
     * @param signal (array): The input signal.
     * @param shift (int): The shift step.
     * @param cof (float): The preemphasising coefficient. 0 equals to no filtering.
     * @returns 0 when successful
     */
    __attribute__((unused)) static int preemphasis(float *signal, size_t signal_size, int shift = 1, float cof = 0.98f)
    {
        if (shift < 0) {
            shift = signal_size + shift;
        }

        // so we need to keep some history
        float *prev_buffer = (float*)ei_dsp_calloc(shift * sizeof(float), 1);

        // signal - cof * xt::roll(signal, shift)
        for (size_t ix = 0; ix < signal_size; ix++) {
            float now = signal[ix];

            // under shift? read from end
            if (ix < static_cast<uint32_t>(shift)) {
                signal[ix] = now - (cof * signal[signal_size - shift + ix]);
            }
            // otherwise read from history buffer
            else {
                signal[ix] = now - (cof * prev_buffer[0]);
            }

            // roll through and overwrite last element
            numpy::roll(prev_buffer, shift, -1);
            prev_buffer[shift - 1] = now;
        }

        ei_dsp_free(prev_buffer, shift * sizeof(float));

        return EIDSP_OK;
    }

    /**
     * Frame a signal into overlapping frames.
     * @param info This is both the base object and where we'll store our results.
     * @param sampling_frequency (int): The sampling frequency of the signal.
     * @param frame_length (float): The length of the frame in second.
     * @param frame_stride (float): The stride between frames.
     * @param zero_padding (bool): If the samples is not a multiple of
     *        frame_length(number of frames sample), zero padding will
     *        be done for generating last frame.
     * @returns EIDSP_OK if OK
     */
    static int stack_frames(stack_frames_info_t *info,
                            uint32_t sampling_frequency,
                            float frame_length,
                            float frame_stride,
                            bool zero_padding)
    {
        if (!info->signal || !info->signal->get_data || info->signal->total_length == 0) {
            EIDSP_ERR(EIDSP_SIGNAL_SIZE_MISMATCH);
        }

        size_t length_signal = info->signal->total_length;
        int frame_sample_length = static_cast<int>(round(static_cast<float>(sampling_frequency) * frame_length));
        frame_stride = round(static_cast<float>(sampling_frequency) * frame_stride);

        volatile int numframes;
        volatile int len_sig;

        if (zero_padding) {
            // Calculation of number of frames
            numframes = static_cast<int>(
                ceil(static_cast<float>(length_signal - frame_sample_length) / frame_stride));

            // Zero padding
            len_sig = static_cast<int>(static_cast<float>(numframes) * frame_stride) + frame_sample_length;

            info->signal->total_length = static_cast<size_t>(len_sig);
        }
        else {
            numframes = static_cast<int>(
                floor(static_cast<float>(length_signal - frame_sample_length) / frame_stride));
            len_sig = static_cast<int>(
                (static_cast<float>(numframes - 1) * frame_stride + frame_sample_length));

            info->signal->total_length = static_cast<size_t>(len_sig);
        }

        // alloc the vector on the heap, will be owned by the info struct
        std::vector<uint32_t> *frame_indices = new std::vector<uint32_t>();

        int frame_count = 0;

        for (size_t ix = 0; ix < static_cast<uint32_t>(len_sig); ix += static_cast<size_t>(frame_stride)) {
            if (++frame_count > numframes) break;

            frame_indices->push_back(ix);
        }

        info->frame_ixs = frame_indices;
        info->frame_length = frame_sample_length;

        return EIDSP_OK;
    }

    /**
     * Calculate the number of stack frames for the settings provided.
     * This is needed to allocate the right buffer size for the output of f.e. the MFE
     * blocks.
     * @param signal_size: The number of frames in the signal
     * @param sampling_frequency (int): The sampling frequency of the signal.
     * @param frame_length (float): The length of the frame in second.
     * @param frame_stride (float): The stride between frames.
     * @param zero_padding (bool): If the samples is not a multiple of
     *        frame_length(number of frames sample), zero padding will
     *        be done for generating last frame.
     * @returns Number of frames required, or a negative number if an error occured
     */
    static int32_t calculate_no_of_stack_frames(
        size_t signal_size,
        uint32_t sampling_frequency,
        float frame_length,
        float frame_stride,
        bool zero_padding)
    {
        size_t length_signal = signal_size;
        int frame_sample_length = static_cast<int>(round(static_cast<float>(sampling_frequency) * frame_length));
        frame_stride = round(static_cast<float>(sampling_frequency) * frame_stride);

        int numframes;

        if (zero_padding) {
            // Calculation of number of frames
            numframes = static_cast<int>(
                ceil(static_cast<float>(length_signal - frame_sample_length) / frame_stride));
        }
        else {
            numframes = static_cast<int>(
                floor(static_cast<float>(length_signal - frame_sample_length) / frame_stride));
        }

        return numframes;
    }

    /**
     * Power spectrum of a frame
     * @param frame Row of a frame
     * @param frame_size Size of the frame
     * @param out_buffer Out buffer, size should be fft_points
     * @param out_buffer_size Buffer size
     * @param fft_points (int): The length of FFT. If fft_length is greater than frame_len, the frames will be zero-padded.
     * @returns EIDSP_OK if OK
     */
    static int power_spectrum(float *frame, size_t frame_size, float *out_buffer, size_t out_buffer_size, uint16_t fft_points)
    {
        if (out_buffer_size != static_cast<size_t>(fft_points / 2 + 1)) {
            EIDSP_ERR(EIDSP_MATRIX_SIZE_MISMATCH);
        }

        int r = numpy::rfft(frame, frame_size, out_buffer, out_buffer_size, fft_points);
        if (r != EIDSP_OK) {
            return r;
        }

        for (size_t ix = 0; ix < out_buffer_size; ix++) {
            out_buffer[ix] = (1.0 / static_cast<float>(fft_points)) *
                (out_buffer[ix] * out_buffer[ix]);
        }

        return EIDSP_OK;
    }

    /**
     * This function performs local cepstral mean and
     * variance normalization on a sliding window. The code assumes that
     * there is one observation per row.
     * @param features_matrix input feature matrix, will be modified in place
     * @param win_size The size of sliding window for local normalization.
     *   Default=301 which is around 3s if 100 Hz rate is
     *   considered(== 10ms frame stide)
     * @param variance_normalization If the variance normilization should
     *   be performed or not.
     * @returns 0 if OK
     */
    static int cmvnw(matrix_t *features_matrix, uint16_t win_size = 301, bool variance_normalization = false)
    {
        uint16_t pad_size = (win_size - 1) / 2;

        int ret;
        float *features_buffer_ptr;

        // mean & variance normalization
        EI_DSP_MATRIX(vec_pad, features_matrix->rows + (pad_size * 2), features_matrix->cols);
        if (!vec_pad.buffer) {
            EIDSP_ERR(EIDSP_OUT_OF_MEM);
        }

        ret = numpy::pad_1d_symmetric(features_matrix, &vec_pad, pad_size, pad_size);
        if (ret != EIDSP_OK) {
            EIDSP_ERR(ret);
        }

        EI_DSP_MATRIX(mean_matrix, vec_pad.cols, 1);
        if (!mean_matrix.buffer) {
            EIDSP_ERR(EIDSP_OUT_OF_MEM);
        }

        EI_DSP_MATRIX(window_variance, vec_pad.cols, 1);
        if (!window_variance.buffer) {
            return EIDSP_OUT_OF_MEM;
        }

        for (size_t ix = 0; ix < features_matrix->rows; ix++) {
            // create a slice on the vec_pad
            EI_DSP_MATRIX_B(window, win_size, vec_pad.cols, vec_pad.buffer + (ix * vec_pad.cols));
            if (!window.buffer) {
                EIDSP_ERR(EIDSP_OUT_OF_MEM);
            }

            ret = numpy::mean_axis0(&window, &mean_matrix);
            if (ret != EIDSP_OK) {
                EIDSP_ERR(ret);
            }

            if (variance_normalization == true) {
                ret = numpy::std_axis0(&window, &window_variance);
                if (ret != EIDSP_OK) {
                    EIDSP_ERR(ret);
                }

                features_buffer_ptr = &features_matrix->buffer[ix * vec_pad.cols];
                for (size_t col = 0; col < vec_pad.cols; col++) {
                    *(features_buffer_ptr) = (*(features_buffer_ptr)-mean_matrix.buffer[col]) /
                                             (window_variance.buffer[col] + FLT_EPSILON);
                    features_buffer_ptr++;
                }
            }

            else {
                features_buffer_ptr = &features_matrix->buffer[ix * vec_pad.cols];
                for (size_t col = 0; col < vec_pad.cols; col++) {
                    *(features_buffer_ptr) = *(features_buffer_ptr)-mean_matrix.buffer[col];
                    features_buffer_ptr++;
                }
            }
        }
        return EIDSP_OK;
    }
};

} // namespace speechpy
} // namespace ei

#endif // _EIDSP_SPEECHPY_PROCESSING_H_
