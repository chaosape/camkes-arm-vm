#
# Copyright 2018, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

cmake_minimum_required(VERSION 3.8.2)

project(WM C)

file(GLOB sources src/*.c src/CMASI/*.c src/CMASI/common/*.c)

DeclareCAmkESComponent(WM
    SOURCES ${sources}
    # XXX: There should be someone to express that this component is dependent
    # on the UART module in a path agnostic fashion.
    INCLUDES ../UART/include
)
