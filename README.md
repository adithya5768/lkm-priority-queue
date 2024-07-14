# Linux Kernel Module - Priority Queue Implementation
This repository contains C code for creating a Linux Kernel Module which provides Priority Queue features for linux users through *ioctl* system calls.

## Priority Queue
Following are the features of the priority queue implemented:
1. Set Capacity
    - Sets the capacity of the priority queue to the given value
2. Insert Value
    - Insert an integer value into the priority queue
3. Insert Priority
    - Set the priority of the last inserted value
4. Get Info
    - Get the priority queue size and the capacity
5. Get Min
    - Extract the value from the priority queue which has the minimum priority
6. Get Max
    - Extract the value from the priority queue which has the maximum priority

## Additional Notes
* To implement the priority queue, Red Black Tree data structure was used as it provides O(1) minimum/maximum extraction.
* The linux kernel implementation of Red Black tree (rbtree) was modified to support extracting both minium and maximum nodes.
* Six *ioctl* system calls were added to support the priority queue features described above.
* Each process can only use a single priority queue at a time.
* Multiple processes (upto 1000) can use their priority queues simultaneously.

## Compilation
This has been compiled and tested in linux 5.6.9. To compile the C code and generate linux kernel module (.ko file), run the following command in the same directory:

```make```

## Installation
To install the linux kernel module after compilation, run the following command in the same directory:

```sudo insmod partb_2_8.ko```

## How to Use
See clinet.c for an example of how to use the installed linux kernel module through *ioctl* system calls.

## Uninstallation
To uninstall the linux kernel module, run the following command:

```sudo rmmod partb_2_8```
