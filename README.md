# mandelbrot-sdl2
SDL2 program to render the Mandelbrot set. I wrote it to have something interesting for getting started with SDL2.

This program renders the Mandelbrot set using [SDL2](https://www.libsdl.org/) for the output.

It supports zoom with '-' and '=' keys and 'w', 'a', 's', 'd' keys for scrolling window.

Rendering is done in multiple threads in parallel, but otherwise it isn't very optimized.

The build is just a plan Makefile that may need adjustment for where SDL2 is installed in your system.

![Screenshot off Mandelbrot SDL2](mandelbrot_screenshot.png?raw=true)

