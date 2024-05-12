# Video Compression with Pthreads

This C program is designed to compress a directory of PPM image files into a single compressed video file using the zlib compression library and multithreading with pthreads.

## Setup
Install zlib on a linux based OS:
$ sudo apt install zlib1g

## Features

- Compresses multiple PPM files in lexicographical order into a single "video.vzip" file
- Uses a producer-consumer model with a linked list buffer to coordinate threads
- Implements thread synchronization with mutex locks, condition variables, and semaphores
- Calculates compression rate and execution time

## Usage

* Compile the code with a C compiler that supports pthreads and zlib.
Run:
$ git clone https://github.com/rfeinberg3/VZIP_Multi-threaded src
$ cd src
$ make
$ make test

The program will create a "video.vzip" file in the current directory containing the compressed video data.

##  Extra 

To run the frame visualizer, first run the VZIP compression program as shown above.
Then run the following command in the same 'src' directory:
$ python vzipVisualizer.py video.vzip



## Code Structure

- `main()` function:
  - Reads the directory and creates a sorted list of PPM files
  - Initializes threads, locks, and other synchronization primitives
  - Creates producer and consumer threads for each file
  - Joins threads and writes the compressed data to the output file
  - Calculates compression rate and execution time

- `adder()` function (producer thread):
  - Reads a PPM file into a buffer
  - Adds the buffer to the linked list buffer for consumers

- `reader()` function (consumer thread):
  - Retrieves a buffer from the linked list
  - Compresses the buffer using zlib
  - Stores the compressed data for writing to the output file

- Helper functions for initializing data structures, locking, and sorting

## Notes

- The code is written for educational purposes and may not be optimized for production use.
- The maximum number of threads and buffer nodes are limited to `MAX_THREADS` (19) and `MAX_NODES` (100), respectively.
- The buffer size for reading and compression is fixed at `BUFFER_SIZE` (1 MB).

# Results

![image](https://github.com/rfeinberg3/VZIP_Multi-threaded/assets/95943957/00806382-54ca-4f33-a6c1-895c1b302714)
