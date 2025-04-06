#!/bin/sh
gcc main.c -o gsimplemixer `pkg-config --cflags --libs gtk4 libpulse libpulse-mainloop-glib`
