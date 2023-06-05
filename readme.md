# OS Paged MMU Simulator

## Project Description

This project was about trying to create a program that simulates a __memory mangement unit__ handling __paged virtual memory__. The purpose is to demonstrate knowledge about how virtual addresses function, what a page table is, how the MMU translates virtual address, and translation lookaside buffers.

## Building 

This program can be built on a posix compliant system using 

> g++ main.cpp -o mmuSim.a

## Commands

The simulation for this program relies on a set of commands so it can read from a text file to operate as if was MMU handling the logic of Bitwise operations.

The list of commands are:

* DUMP_MMU - prints the contents of the __translation lookaside buffer__ to the screen
* DUMP_PT - prints the contents of the __linear page table__ to the screen
* Read - read from a specified virtual address
* Write - write to a specified virtual address

anything else is an undefined commands and ends the program.

## Linear Page Table

The make up of are simulated page table is:

1. the page number
2. if the page is present or in swap space
3. whether the page is dirty or clean (if it has been written to or not)
4. the final value is the physical frame number for the current page.

all pages that are printed to the screen are the ones that are present in memory and not the swap buffer.

## Swap Space

The swap device is simulated and thus does not technically exist. However, the project specs required the concept of swaping pages from memory (since there is only so many pages that can fit) and the swap device. The swap space is simulated in this manner:

* All pages start in swap space
* A page that is dirty must be "written back" to the swap device (simulated so nothing happaned except extra text written to the output)
* every action of the swap space must be printed to the programs output
* there is no selection algorithm for choosing a page to eject from memory

