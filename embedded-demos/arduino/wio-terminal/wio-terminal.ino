/**
 * Wio Terminal Keyword Spotting Demo
 * 
 * Perform continuous audio keyword spotting using Edge Impulse library.
 * 
 * Author: Shawn Hymel
 * Date: February 28, 2021
 * Updated: March 19, 2022
 * 
 * This demo uses the internal microphone on the Wio Terminal. Note that
 * the internal microphone is configured to measure sound levels and does
 * not work as well for recording raw audio. It will work decently enough 
 * for this demo. Seeed Studio has promised a hardware fix for the microphone
 * on the Wio Terminal v2.
 * 
 * You will need to install the Edge Impulse .zip file as a library. This
 * sketch works with the following .zip library: 
 * https://github.com/ShawnHymel/ei-keyword-spotting/tree/master/embedded-demos/arduino/arduino-nano-33-ble-sense
 * Download the .zip file and install as Arduino library. Don't worry about
 * the Nano 33 BLE Sense examples, as we just need the underlying library
 * for this demo.
 * 
 * If you download/install a .zip file from a different Edge Impulse project,
 * you will need to change the #include below to point to that library.
 * 
 * Based on: Arduino Nano 33 BLE Sense continuous microphone demo
 * by Edge Impulse
 * 
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

// If your target is limited in memory remove this macro to save 10K RAM
#define EIDSP_QUANTIZE_FILTERBANK   0

/**
 * Define the number of slices per model window. E.g. a model window of 1000 ms
 * with slices per model window set to 4. Results in a slice size of 250 ms.
 * For more info: https://docs.edgeimpulse.com/docs/continuous-audio-sampling
 */
#define EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW 3

// Change this to match the Edge Impulse library that you downloaded
#include <keyword-spotting-03_inference.h>

// Other includes
#include "TFT_eSPI.h" // This should come with Wio Terminal package install

// Settings
#define DEBUG 1                 // Enable pin pulse during ISR  
enum { ADC_BUF_LEN = 1600 };    // Size of one of the DMA double buffers
static const int debug_pin = 1; // Toggles each DAC ISR (if DEBUG is set to 1)
static const float maf_threshold = 0.7;

// MAF calculation values
typedef struct {
  float prev = 0.0;
  float val = 0.0;
  float maf = 0.0;
} maf_result_t;

// DMAC descriptor structure
typedef struct {
  uint16_t btctrl;
  uint16_t btcnt;
  uint32_t srcaddr;
  uint32_t dstaddr;
  uint32_t descaddr;
} dmacdescriptor;

// Audio buffers, pointers and selectors
typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

// Globals - DMA and ADC
volatile uint8_t recording = 0;
volatile boolean results0Ready = false;
volatile boolean results1Ready = false;
uint16_t adc_buf_0[ADC_BUF_LEN];    // ADC results array 0
uint16_t adc_buf_1[ADC_BUF_LEN];    // ADC results array 1
volatile dmacdescriptor wrb[DMAC_CH_NUM] __attribute__ ((aligned (16)));          // Write-back DMAC descriptors
dmacdescriptor descriptor_section[DMAC_CH_NUM] __attribute__ ((aligned (16)));    // DMAC channel descriptors
dmacdescriptor descriptor __attribute__ ((aligned (16)));                         // Place holder descriptor

// Globals - Edge Impulse
static inference_t inference;
static bool record_ready = false;
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);

// Globals - LCD
TFT_eSPI tft;

/*******************************************************************************
 * Interrupt Service Routines (ISRs)
 */

/**
 * @brief      Copy sample data in selected buf and signal ready when buffer is full
 *
 * @param[in]  *buf  Pointer to source buffer
 * @param[in]  buf_len  Number of samples to copy from buffer
 */
static void audio_rec_callback(uint16_t *buf, uint32_t buf_len) {

  static uint32_t idx = 0;

  // Copy samples from DMA buffer to inference buffer
  if (recording) {
    for (uint32_t i = 0; i < buf_len; i++) {
  
      // Convert 12-bit unsigned ADC value to 16-bit PCM (signed) audio value
      inference.buffers[inference.buf_select][inference.buf_count++] =
          ((int16_t)buf[i] - 2048) * 16;
  
      // Swap double buffer if necessary
      if (inference.buf_count >= inference.n_samples) {
        inference.buf_select ^= 1;
        inference.buf_count = 0;
        inference.buf_ready = 1;
      }
    }
  }
}

/**
 * Interrupt Service Routine (ISR) for DMAC 1
 */
void DMAC_1_Handler() {

  static uint8_t count = 0;
  static uint16_t idx = 0;

  // Check if DMAC channel 1 has been suspended (SUSP)
  if (DMAC->Channel[1].CHINTFLAG.bit.SUSP) {

     // Debug: make pin high before copying buffer
#if DEBUG
    digitalWrite(debug_pin, HIGH);
#endif

    // Restart DMAC on channel 1 and clear SUSP interrupt flag
    DMAC->Channel[1].CHCTRLB.reg = DMAC_CHCTRLB_CMD_RESUME;
    DMAC->Channel[1].CHINTFLAG.bit.SUSP = 1;

    // See which buffer has filled up, and dump results into large buffer
    if (count) {
      audio_rec_callback(adc_buf_0, ADC_BUF_LEN);
    } else {
      audio_rec_callback(adc_buf_1, ADC_BUF_LEN);
    }

    // Flip to next buffer
    count = (count + 1) % 2;

    // Debug: make pin low after copying buffer
#if DEBUG
    digitalWrite(debug_pin, LOW);
#endif
  }
}

/*******************************************************************************
 * Functions
 */

// Configure DMA to sample from ADC at regular interval
// I'm sorry everything is hardcoded. I don't have time to make a library.
// And I just now realized that the Adafruit_ZeroDMA library would likely work.
// This is all based on MartinL's work from these two posts:
// https://forum.arduino.cc/index.php?topic=685347.0
// and https://forum.arduino.cc/index.php?topic=709104.0
void config_dma_adc() {
  
  // Configure DMA to sample from ADC at a regular interval (triggered by timer/counter)
  DMAC->BASEADDR.reg = (uint32_t)descriptor_section;                          // Specify the location of the descriptors
  DMAC->WRBADDR.reg = (uint32_t)wrb;                                          // Specify the location of the write back descriptors
  DMAC->CTRL.reg = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0xf);                // Enable the DMAC peripheral
  DMAC->Channel[1].CHCTRLA.reg = DMAC_CHCTRLA_TRIGSRC(TC5_DMAC_ID_OVF) |      // Set DMAC to trigger on TC5 timer overflow
                                 DMAC_CHCTRLA_TRIGACT_BURST;                  // DMAC burst transfer
  descriptor.descaddr = (uint32_t)&descriptor_section[1];                     // Set up a circular descriptor
  descriptor.srcaddr = (uint32_t)&ADC1->RESULT.reg;                           // Take the result from the ADC0 RESULT register
  descriptor.dstaddr = (uint32_t)adc_buf_0 + sizeof(uint16_t) * ADC_BUF_LEN;  // Place it in the adc_buf_0 array
  descriptor.btcnt = ADC_BUF_LEN;                                             // Beat count
  descriptor.btctrl = DMAC_BTCTRL_BEATSIZE_HWORD |                            // Beat size is HWORD (16-bits)
                      DMAC_BTCTRL_DSTINC |                                    // Increment the destination address
                      DMAC_BTCTRL_VALID |                                     // Descriptor is valid
                      DMAC_BTCTRL_BLOCKACT_SUSPEND;                           // Suspend DMAC channel 0 after block transfer
  memcpy(&descriptor_section[0], &descriptor, sizeof(descriptor));            // Copy the descriptor to the descriptor section
  descriptor.descaddr = (uint32_t)&descriptor_section[0];                     // Set up a circular descriptor
  descriptor.srcaddr = (uint32_t)&ADC1->RESULT.reg;                           // Take the result from the ADC0 RESULT register
  descriptor.dstaddr = (uint32_t)adc_buf_1 + sizeof(uint16_t) * ADC_BUF_LEN;  // Place it in the adc_buf_1 array
  descriptor.btcnt = ADC_BUF_LEN;                                             // Beat count
  descriptor.btctrl = DMAC_BTCTRL_BEATSIZE_HWORD |                            // Beat size is HWORD (16-bits)
                      DMAC_BTCTRL_DSTINC |                                    // Increment the destination address
                      DMAC_BTCTRL_VALID |                                     // Descriptor is valid
                      DMAC_BTCTRL_BLOCKACT_SUSPEND;                           // Suspend DMAC channel 0 after block transfer
  memcpy(&descriptor_section[1], &descriptor, sizeof(descriptor));            // Copy the descriptor to the descriptor section

  // Configure NVIC
  NVIC_SetPriority(DMAC_1_IRQn, 0);    // Set the Nested Vector Interrupt Controller (NVIC) priority for DMAC1 to 0 (highest)
  NVIC_EnableIRQ(DMAC_1_IRQn);         // Connect DMAC1 to Nested Vector Interrupt Controller (NVIC)

  // Activate the suspend (SUSP) interrupt on DMAC channel 1
  DMAC->Channel[1].CHINTENSET.reg = DMAC_CHINTENSET_SUSP;
  
  // Configure ADC
  ADC1->INPUTCTRL.bit.MUXPOS = ADC_INPUTCTRL_MUXPOS_AIN12_Val; // Set the analog input to ADC0/AIN2 (PB08 - A4 on Metro M4)
  while(ADC1->SYNCBUSY.bit.INPUTCTRL);                // Wait for synchronization
  ADC1->SAMPCTRL.bit.SAMPLEN = 0x00;                  // Set max Sampling Time Length to half divided ADC clock pulse (2.66us)
  while(ADC1->SYNCBUSY.bit.SAMPCTRL);                 // Wait for synchronization 
  ADC1->CTRLA.reg = ADC_CTRLA_PRESCALER_DIV128;       // Divide Clock ADC GCLK by 128 (48MHz/128 = 375kHz)
  ADC1->CTRLB.reg = ADC_CTRLB_RESSEL_12BIT |          // Set ADC resolution to 12 bits
                    ADC_CTRLB_FREERUN;                // Set ADC to free run mode       
  while(ADC1->SYNCBUSY.bit.CTRLB);                    // Wait for synchronization
  ADC1->CTRLA.bit.ENABLE = 1;                         // Enable the ADC
  while(ADC1->SYNCBUSY.bit.ENABLE);                   // Wait for synchronization
  ADC1->SWTRIG.bit.START = 1;                         // Initiate a software trigger to start an ADC conversion
  while(ADC1->SYNCBUSY.bit.SWTRIG);                   // Wait for synchronization

  // Enable DMA channel 1
  DMAC->Channel[1].CHCTRLA.bit.ENABLE = 1;

  // Configure Timer/Counter 5
  GCLK->PCHCTRL[TC5_GCLK_ID].reg = GCLK_PCHCTRL_CHEN |        // Enable perhipheral channel for TC5
                                   GCLK_PCHCTRL_GEN_GCLK1;    // Connect generic clock 0 at 48MHz
   
  TC5->COUNT16.WAVE.reg = TC_WAVE_WAVEGEN_MFRQ;               // Set TC5 to Match Frequency (MFRQ) mode
  TC5->COUNT16.CC[0].reg = 3000 - 1;                          // Set the trigger to 16 kHz: (4Mhz / 16000) - 1
  while (TC5->COUNT16.SYNCBUSY.bit.CC0);                      // Wait for synchronization

  // Start Timer/Counter 5
  TC5->COUNT16.CTRLA.bit.ENABLE = 1;                          // Enable the TC5 timer
  while (TC5->COUNT16.SYNCBUSY.bit.ENABLE);                   // Wait for synchronization
}

/**
 * @brief      Printf function uses vsnprintf and output using Arduino Serial
 *
 * @param[in]  format     Variable argument list
 */
void ei_printf(const char *format, ...) {
    static char print_buf[1024] = { 0 };

    va_list args;
    va_start(args, format);
    int r = vsnprintf(print_buf, sizeof(print_buf), format, args);
    va_end(args);

    if (r > 0) {
        Serial.write(print_buf);
    }
}

/**
 * @brief      Wait for full buffer
 *
 * @return     False if buffer overrun
 */
static bool microphone_inference_record(void) {
  bool ret = true;

  if (inference.buf_ready == 1) {
      ei_printf(
          "Error sample buffer overrun. Decrease the number of slices per model window "
          "(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)\n");
      ret = false;
  }

  // TODO: Make this non-blocking (use RTOS?)
  while (inference.buf_ready == 0) {
      delay(1);
  }

  inference.buf_ready = 0;

  return ret;
}

/**
 * Get raw audio signal data
 */
static int microphone_audio_signal_get_data(size_t offset, 
                                            size_t length, 
                                            float *out_ptr) {
    numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);

    return 0;
}

/**
 * @brief      Stop PDM and release buffers
 */
static void microphone_inference_end(void) {
  // TODO: Stop DMA and ADC

  // Free up double buffer
  free(inference.buffers[0]);
  free(inference.buffers[1]);

}

/**
 * @brief     Print string to LCD
 * 
 * @param[in] String as char array
 */
void lcd_print_string(const char str[]) {

  // Disable recording for 1-second hold-off
  recording = 0;

  // Draw string
  tft.fillScreen(TFT_BLACK);
  tft.drawString(str, 40, 80);

  // Re-enable recording
  recording = 1;
}

/*******************************************************************************
 * Main
 */

void setup() {

  // Configure pin to toggle on DMA interrupt
#if DEBUG
  pinMode(debug_pin, OUTPUT);
#endif

  // Configure serial port for debugging
  Serial.begin(115200);

  // Configure LCD
  tft.begin();
  tft.setRotation(3);
  tft.setFreeFont(&FreeSansBoldOblique24pt7b);
  tft.fillScreen(TFT_BLACK);

  // Print summary of inferencing settings (from model_metadata.h)
  ei_printf("Inferencing settings:\n");
  ei_printf("\tInterval: %.2f ms.\n", (float)EI_CLASSIFIER_INTERVAL_MS);
  ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
  ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) /
                                          sizeof(ei_classifier_inferencing_categories[0]));

  // Initialize classifier
  run_classifier_init();

  // Create double buffer for inference
  inference.buffers[0] = (int16_t *)malloc(EI_CLASSIFIER_SLICE_SIZE * 
      sizeof(int16_t));
  if (inference.buffers[0] == NULL) {
    ei_printf("ERROR: Failed to create inference buffer 0");
    return;
  }
  inference.buffers[1] = (int16_t *)malloc(EI_CLASSIFIER_SLICE_SIZE * 
      sizeof(int16_t));
  if (inference.buffers[1] == NULL) {
    ei_printf("ERROR: Failed to create inference buffer 1");
    free(inference.buffers[0]);
    return;
  }

  // Set inference parameters
  inference.buf_select = 0;
  inference.buf_count = 0;
  inference.n_samples = EI_CLASSIFIER_SLICE_SIZE;
  inference.buf_ready = 0;

  // Configure DMA to sample from ADC at 16kHz (start sampling immediately)
  config_dma_adc();

  // Start recording to inference buffers
  recording = 1;
}

void loop() { 

  // Wait until buffer is full
  bool m = microphone_inference_record();
  if (!m) {
    ei_printf("ERROR: Audio buffer overrun\r\n");
    return;
  }

  // Do classification (i.e. the inference part)
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
  signal.get_data = &microphone_audio_signal_get_data;
  ei_impulse_result_t result = { 0 };
  EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
  if (r != EI_IMPULSE_OK) {
      ei_printf("ERROR: Failed to run classifier (%d)\r\n", r);
      return;
  }

  // YOUR CODE GOES HERE! (if you want to do something with inference results)

  // ***Example below: print keyword to LCD***
  
  static int idx_prev;
  int idx;

  // Store value and MAF results for keywords
  static maf_result_t mafs[EI_CLASSIFIER_LABEL_COUNT];

  // Calculate 2-point moving average filter (MAF) for all keyword labels
  for (int ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    mafs[ix].val = result.classification[ix].value;
    mafs[ix].maf = (mafs[ix].prev + mafs[ix].val) / 2;
    mafs[ix].prev = mafs[ix].val;
  }

  // Figure out if any MAF values surpass threshold
  // Ignore "_noise" (ix = 0) and "_unknown" (ix = 1)
  for (int ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    if (mafs[ix].maf > maf_threshold) {
      idx = ix;
      break;
    }
  }

  // Print label to LCD if predicted class is different from last iteration
  // Don't print anything for "_noise" (idx == 0) or "_unknown" (idx == 1)
  if (idx != idx_prev) {
    if (idx >= 2) {
      lcd_print_string(result.classification[idx].label);
    } else {
      lcd_print_string("");
    }
  }
  idx_prev = idx;

  // ***End example***

  // Print output prediction results (once every 3 predictions)
  if(++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW >> 1)) {
    
    // Comment this section out if you don't want to see the raw scores
    ei_printf("Predictions (DSP: %d ms, NN: %d ms)\r\n", result.timing.dsp, result.timing.classification);
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: %.5f\r\n", result.classification[ix].label, result.classification[ix].value);
    }
    print_results = 0;
  }
}
