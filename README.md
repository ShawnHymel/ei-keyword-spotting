# Keyword Spotting with Edge Impulse

%%%Nucleo and Mic

This repository is a collection of tools and demo projects to help you get started creating your own embedded keyword spotting system with machine learning. Keyword spotting (or wake word) is a form of voice recognition that allows computers (or microcontrollers) to respond to spoken words.

Please note that because we are targeting embedded systems (namely, microcontrollers), this demo is very limited. You will likely only be able to train a system to recognize 1 or 2 words at a time. Anything more than that will require a more powerful microcontroller with more memory.

## Repository Contents

* **embedded-demos/** - Collection of keyword spotting projects for various microcontroller development boards
* **images/** - I need to put pictures somewhere
* **dataset-curation.py** - Run this locally to combine custom wake words with the Google Speech Commands dataset and mix samples with background noise
* **ei-audio-dataset-curation.ipynb** - Run this in Google Colab if you want to do curation remotely
* **utils.py** - A few functions to help with dataset curation

## Getting Started

### Data Collection

This project relies on the Google Speech Commands dataset as a starting point for your keyword spotting system. You may choose any word(s) from that set to be your keyword (or wake word). Additionally, you may record your own samples to use in addition to the Google Speech Commands dataset.

If you decide to record your own samples, please follow these recommended guidelines:
* Samples should be 1 second long, mono, in raw (.wav) format
* Each sample should contain only 1 utterance of the keyword
* Samples of each category need to be contained in a directory named for that category
* Try to get at least 50 samples. Over 100 is better, over 1000 is best
* Samples should contain a variety of pronunciations, accents, inflections, etc. for a more robust model

Store your samples in a directory structure as follows. The filename of each sample does not matter, but the name of the category directory does matter (it will be read by the scripts to determing category names).

```
|- custom_keywords
|--- keyword_1
|----- 000.wav
|----- 001.wav
|----- ...
|--- keyword_2
|----- 000.wav
|----- 001.wav
|----- ...
|--- ...
```

### Data Curation

Once we have samples, we need to curate our dataset. To do this, we're going to combine any custom samples with the Google Speech Commands dataset, mix the samples with background noise, and upload them to Edge Impulse. There are two ways to do this: run a Jupyter Notebook remotely in Google Colab or run the *data-curation.py* script locally on your machine.

#### Option 1: Google Colab

This is the best option if you have a slow Internet connection or don't want to mess with Python on your computer.

Sign up for an account on [Edge Impulse](https://www.edgeimpulse.com/). Create a new project. Go into that project, select **Dashboard > Keys**. Copy the API key.

%%%screen - ei key

Create a [Google Gmail](https://gmail.com/) account, if you do not already have one.

Click on the **ei-audio-dataset-curation.ipynb** link and click on the **Open in Colab** link to run the Notebook in Colab.

Alternatively, just click this: [![Open In Colab <](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/ShawnHymel/ei-keyword-spotting/blob/master/ei-audio-dataset-curation.ipynb)

Follow the instructions in the Notebook to curate your own dataset.

Note that in the *Settings* cell, you will need to adjust a few variables.
* Leave `CUSTOM_KEYWORDS_PATH` as an empty string (`""`) if you wish to just use the Google Speech Commands dataset or set it to the location of your custom keyword samples directory
* Paste in your Edge Impulse API key (as a string) for `EI_API_KEY`
* Set your desired target keywords as a comma-separated string for `TARGETS`

Run the rest of the cells (use 'shift' + 'enter' to run a cell)!

#### Option 2: Local Curation Script

TODO

## License

Unless otherwise noted (e.g. STM32CubeIDE projects have their own licenses), code is licensed under the MIT license:

```
The MIT License (MIT)

Copyright (c) 2020 Shawn Hymel

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```