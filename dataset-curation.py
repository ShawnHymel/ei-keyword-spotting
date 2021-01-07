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
    -t "go, stop" -n 1500 -w 1.0 -g 0.1 -s 1.0 -r 16000 -e PCM_16
    -b "../../Python/datasets/background_noise"
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

Only WAV files are supported at this time.

Output directory (given by -o parameter) will be divided up into targets, noise,
and unknown. For example:
    - keywords_curated
    |--- _noise
    |--- _unknown
    |--- go
    |--- stop
    
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
from os.path import isdir, join, exists, splitext

import shutil
import librosa
import soundfile as sf
import numpy as np

import utils

# Authorship
__author__ = "Shawn Hymel"
__copyright__ = "Copyright 2020, Shawn Hymel"
__license__ = "MIT"
__version__ = "0.1"

# Settings
unknown_dir_name = "_unknown"
bg_dir_name = "_noise"

################################################################################
# Functions

# Mix audio and random snippet of background noise
def mix_audio(word_path=None, 
                bg_path=None, 
                word_vol=1.0, 
                bg_vol=1.0, 
                sample_time=1.0,
                sample_rate=16000):
    """
    Read in a wav file and background noise file. Resample and adjust volume as
    necessary.
    """
    
    # If no word file is given, just return random background noise
    if word_path == None:
        waveform = [0] * int(sample_time * sample_rate)
        fs = sample_rate
    else:

        # Open wav file, resample, mix to mono
        waveform, fs = librosa.load(word_path, sr=sample_rate, mono=True)
        
        # Pad 0s on the end if not long enough
        if len(waveform) < sample_time * sample_rate:
            waveform = np.append(waveform, np.zeros(int((sample_time * 
                sample_rate) - len(waveform))))

        # Truncate if too long
        waveform = waveform[:int(sample_time * sample_rate)]

    # If no background noise is given, just return the waveform
    if bg_path == None:
        return waveform

    # Open background noise file
    bg_waveform, fs = librosa.load(bg_path, sr=fs)

    # Pick a random starting point in background file
    max_end = len(bg_waveform) - int(sample_time * sample_rate)
    start_point = random.randint(0, max_end)
    end_point = start_point + int(sample_time * sample_rate)
    
    # Mix the two sound samples (and multiply by volume)
    waveform = [0.5 * word_vol * i for i in waveform] + \
                (0.5 * bg_vol * bg_waveform[start_point:end_point])

    return waveform

################################################################################
# Main

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
                    default=1500,
                    help="Number of mixed samples to place in each output "
                            "category subdirectory (default: 1500)")
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
                    default=0.1,
                    help="Relative volume to multiply each background noise by "
                            "(default: 0.1)")
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
parser.add_argument('-b',
                    '--bg_dir',
                    action='store',
                    dest='bg_dir',
                    type=str,
                    required=True,
                    help="Directory where the background noise samples are "
                            "stored")
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
word_vol = args.word_vol
bg_vol = args.bg_vol
sample_time = args.sample_time
sample_rate = args.sample_rate
bit_depth = args.bit_depth
bg_dir = args.bg_dir
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
    try:
        for name in listdir(directory):
          if isdir(join(directory, name)):
              word_list.append(name)
    except OSError:
        print("No directory named '" + str(directory) + "'. Ignoring.")

# Remove duplicates from list
word_list = list(dict.fromkeys(word_list))

# Make target list and make sure each target word appears in our list of words
target_list = [word.strip() for word in targets.split(',')]
for target in target_list:
    if target not in word_list:
        print("ERROR: Target word '" + target + "' not found as subdirectory "
                "in input directories. Exiting.")
        exit()

# Remove targets from word list to create "unknown" list
unknown_list = []
[unknown_list.append(name) for name in word_list if name not in target_list]

# Seed (use system time)
random.seed()

# Figure out the number of digits needed in for filename counter
num_digits = len(str(num_samples))

###
# Save clips of background noise

# Create _background subdirectory
out_subdir = join(out_dir, bg_dir_name)
makedirs(out_subdir)

# Create list of background noise files
bg_paths = []
for bg_file in listdir(bg_dir):
    _, extension = splitext(bg_file)
    if (extension == ".wav" or extension == ".WAV"):
        bg_paths.append(join(bg_dir, bg_file))

# Randomize list of paths and take first n paths
random.shuffle(bg_paths)
bg_paths = bg_paths[:num_samples]
num_bg_files = len(bg_paths)

# Print what we're doing and show progress bar
print("Gathering random background noise snippets (" + str(num_samples) +
        " files)")
utils.print_progress_bar(   0, 
                            num_samples, 
                            prefix="Progress:", 
                            suffix="Complete", 
                            length=50)

# Go through each background file, taking a random sample from different points
for i in range(num_samples):

    # Get random snippet from background noise (round robin bg files)
    waveform = mix_audio(   word_path=None, 
                            bg_path=bg_paths[i % num_bg_files], 
                            word_vol=word_vol, 
                            bg_vol=bg_vol, 
                            sample_time=sample_time,
                            sample_rate=sample_rate)
    
    # Save to new file
    filename = bg_dir_name + "." + str(i).zfill(num_digits) + ".wav"
    sf.write(   join(out_subdir, filename), 
                waveform, 
                sample_rate, 
                subtype=bit_depth)

    # Update progress bar
    utils.print_progress_bar(   i + 1, 
                                num_samples, 
                                prefix="Progress:", 
                                suffix="Complete", 
                                length=50)

###
# Mix target words

# Create a list of target word locations
for target in target_list:

    # Create target word subdirectory
    out_subdir = join(out_dir, target)
    makedirs(out_subdir)

    # Create list of paths with target word audio files
    file_paths = []
    for directory in in_dirs:
        in_subdir = join(directory, target)
        if isdir(in_subdir):
            for word_filename in listdir(in_subdir):
                _, extension = splitext(word_filename)
                if (extension == ".wav" or extension == ".WAV"):
                    file_paths.append(join(in_subdir, word_filename))
    
    # Randomize list of paths and take first n paths
    random.shuffle(file_paths)
    file_paths = file_paths[:num_samples]

    # Print what we're doing and show progress bar
    print("Mixing: " + str(target) + " (" + str(num_samples) + " files)") 
    utils.print_progress_bar(   0, 
                                num_samples, 
                                prefix="Progress:", 
                                suffix="Complete", 
                                length=50)

    # Go through each target word file and mix it with a random noise snippet
    for i in range(num_samples):

        # Randomly choose background noise from list
        # %%%TODO: Better way to do this? Round robin from bg list?
        bg_path = bg_paths[random.randrange(num_bg_files)]
        
        # Mix target audio file and background noise file. Use round-robin to
        # add duplicate target files (if less than number of desired samples)
        waveform = mix_audio(   word_path=file_paths[i % len(file_paths)], 
                                bg_path=bg_path, 
                                word_vol=word_vol, 
                                bg_vol=bg_vol, 
                                sample_time=sample_time,
                                sample_rate=sample_rate)

        # Save to new file
        filename = target + "." + str(i).zfill(num_digits) + ".wav"
        sf.write(   join(out_subdir, filename),
                    waveform,
                    sample_rate,
                    subtype=bit_depth)
                    
        # Update progress bar
        utils.print_progress_bar(   i + 1, 
                                    num_samples, 
                                    prefix="Progress:", 
                                    suffix="Complete", 
                                    length=50)

###
# Mix words in "unknown" category

# Create _background subdirectory
out_subdir = join(out_dir, unknown_dir_name)
makedirs(out_subdir)

# Create list of paths with target word audio files
file_paths = []
for directory in in_dirs:
    try:
        for in_subdir in listdir(directory):
            if (isdir(join(directory, in_subdir))) and \
            (in_subdir in unknown_list):
                for word_filename in listdir(join(directory, in_subdir)):
                    _, extension = splitext(word_filename)
                    if (extension == ".wav" or extension == ".WAV"):
                        file_paths.append(join( directory, 
                                                in_subdir, 
                                                word_filename))
    except OSError:
        print("No directory named '" + str(directory) + "'. Ignoring.")

# Randomize list of paths and take first n paths
random.shuffle(file_paths)
file_paths = file_paths[:num_samples]

# Only proceed if we have one or more files to work with
if len(file_paths) > 0:

    # Print what we're doing and show progress bar
    print("Mixing: " + str(unknown_dir_name) + " (" + str(num_samples) + 
          " files)")
    utils.print_progress_bar(   0, 
                                num_samples, 
                                prefix="Progress:", 
                                suffix="Complete", 
                                length=50)

    # Go through each target word file and mix it with a random noise snippet
    for i in range(num_samples):

        # Randomly choose background noise from list
        # %%%TODO: Better way to do this? Round robin from bg list?
        bg_path = bg_paths[random.randrange(num_bg_files)]
        
        # Mix target audio file and background noise file. Use round-robin to
        # add duplicate target files (if less than number of desired samples)
        waveform = mix_audio(   word_path=file_paths[i % len(file_paths)], 
                                bg_path=bg_path, 
                                word_vol=word_vol, 
                                bg_vol=bg_vol, 
                                sample_time=sample_time,
                                sample_rate=sample_rate)

        # Save to new file
        filename = unknown_dir_name + "." + str(i).zfill(num_digits) + ".wav"
        sf.write(   join(out_subdir, filename),
                    waveform,
                    sample_rate,
                    subtype=bit_depth)
                    
        # Update progress bar
        utils.print_progress_bar(   i + 1, 
                                    num_samples, 
                                    prefix="Progress:", 
                                    suffix="Complete", 
                                    length=50)

# Say we're done
print("Done!")
exit()
