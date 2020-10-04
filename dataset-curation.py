#!/usr/bin/env python3

"""
Dataset Curation

Script to select which audio files should be included in the training/test 
dataset. This allows you to limit the total number of files (faster training but
lower accuracy). It also mixes in random samples of background noise to allow 
for a more robust model and separates out target keywords from other words.

You will need the following packages (install via pip):
 * numpy
 * librosa
 * soundfile
 * shutil

Example call:
python dataset-curation.py
    -t "stop, go" -n 500 -m 3 -w 1.0 -g 0.2 -s 1.0 -r 16000 -e PCM_16
    -o "../../Python/datasets/keywords_curated"
    "../../../Python/datasets/speech_commands_dataset"
    "../../Python/datasets/custom_keywords"     

Note that each input directory must be divided into "word" subdirectories. For
example:
    - speech_commands_dataset
    |--- backward
    |--- bed
    |--- bird
    |--- ...

At this time, the script does not support input directories that share
subdirectory names with other input directories' subdirectories (i.e. put all
your word samples in a single subdirectory among the input directories, as the
script does not handle something like input_dir_1/hello and input_dir_2/hello).

Output directory (given by -o parameter) will be divided up into targets, noise,
and unknown. For example:
    - keywords_curated
    |--- go
    |--- noise
    |--- stop
    |--- unknown

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
"""

import time
import random
import argparse
from os import makedirs, listdir, rename
from os.path import isdir, join, exists

import shutil

import utils

# Authorship
__author__ = "Shawn Hymel"
__copyright__ = "Copyright 2020, Shawn Hymel"
__license__ = "MIT"
__version__ = "0.1"

###
# Parse command line arguments

# Script arguments
parser = argparse.ArgumentParser(description="Audio mixing tool that "
                                "combines spoken word examples with random "
                                "bits of background noise to create a dataset "
                                "for use with keyword spotting training.")

parser.add_argument('-t',
                   '--targets',
                   action='store',
                   dest='targets',
                   type=str,
                   required=True,
                   help="List of target words, separated by commas. Spaces not "
                        "allowed--they will be treated as commas.")
parser.add_argument('-n',
                    '--num_samples',
                    action='store',
                    dest='num_samples',
                    type=int,
                    default=500,
                    help="Number of samples to randomly draw from each target "
                            "keyword. Total number of samples created in each "
                            "output directory is m * n (default: 500)")
parser.add_argument('-m',
                    '--num_bg_samples',
                    action='store',
                    dest='num_bg_samples',
                    type=int,
                    default=3,
                    help="Number of random clips to take from each background "
                            "noise file (default: 3)")
parser.add_argument('-w',
                    '--word_vol',
                    action='store',
                    dest='word_vol',
                    type=float,
                    default=1.0,
                    help="Relative volume to multiply each word by (default: "
                         "1.0)")
parser.add_argument('-g',
                    '--bg_vol',
                    action='store',
                    dest='bg_vol',
                    type=float,
                    default=0.2,
                    help="Relative volume to multiply each background noise by "
                            "(default: 0.2)")
parser.add_argument('-s',
                    '--sample_time',
                    action='store',
                    dest='sample_time',
                    type=float,
                    default=1.0,
                    help="Time (seconds) of each output clip (default: 1.0)")
parser.add_argument('-r',
                    '--sample_rate',
                    action='store',
                    dest='sample_rate',
                    type=int,
                    default=16000,
                    help="Sample rate (Hz) of each output clip (default: "
                            "16000)")
parser.add_argument('-e',
                    '--bit_depth',
                    action='store',
                    dest='bit_depth',
                    type=str,
                    choices=['PCM_16', 'PCM_24', 'PCM_32', 'PCM_U8', 'FLOAT',
                             'DOUBLE'],
                    default='PCM_16',
                    help="Bit depth of each sample (default: PCM_16)")
parser.add_argument('-o',
                   '--out_dir',
                   action='store',
                   dest='out_dir',
                   type=str,
                   required=True,
                   help="Directory where the mixed audio samples are to be "
                        "stored")
parser.add_argument('in_dirs',
                    metavar='d',
                    type=str,
                    nargs='+',
                    help="List of source directories to include")

# Parse arguments
args = parser.parse_args()
targets = args.targets
num_samples = args.num_samples
num_bg_samples = args.num_bg_samples
word_vol = args.word_vol
bg_vol = args.bg_vol
sample_time = args.sample_time
sample_rate = args.sample_rate
bit_depth = args.bit_depth
out_dir = args.out_dir
in_dirs = args.in_dirs

###
# Welcome screen

# Print tool welcome message
print("-----------------------------------------------------------------------")
print("Keyword Dataset Curation Tool")
print("v" + __version__)
print("-----------------------------------------------------------------------")

###
# Set up directories

# Delete output directory if it already exists
if isdir(out_dir):
    print("WARNING: Output directory already exists:")
    print(out_dir)
    print("This tool will delete the output directory and everything in it.")
    resp = utils.query_yes_no("Continue?")
    if resp:
        print("Deleting and recreating output directory.")
        #rename(out_dir, out_dir + '_') # One way to deal with OS blocking rm
        shutil.rmtree(out_dir)
        time.sleep(2.0)
    else:
        print("Please delete directory to continue. Exiting.")
        exit()

# Create output dir
if not exists(out_dir):
    makedirs(out_dir)
else:
    print("ERROR: Output directory could not be deleted. Exiting.")
    exit()

# Create a list of possible words from the subdirectories
word_list = []
for directory in in_dirs:
    for name in listdir(directory):
        if isdir(join(directory, name)):
            word_list.append(name)

# Remove duplicates from list
word_list = list(dict.fromkeys(word_list))

print(word_list)