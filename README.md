**Simple Wayland Image Viewer**

A fork of [sxiv](https://github.com/xyb3rt/sxiv) for wayland.

Configuration
--------

In sxiv, font, background and foreground color are configured using Xresources.
Unfortunately wayland doesn't have a equivalent to Xresources, so swiv tries to
parse the Xresources file directly to get the values from the file. The parsing
isn't the same and might not work with certain files / features, mainly one
involving the c preprocessor (cpp). The `$XRES_PATH` environment variable may
be a path to an Xresources file. If the variable isn't set, `$HOME/.Xresources`
and `$HOME/.Xdefaults` are tried.

There are also, new flags added, `-F` for font, `-B` for background color and
`-C` for foreground color.

Features
--------

* Basic image operations, e.g. zooming, panning, rotating
* Customizable key and mouse button mappings (in *config.h*)
* Thumbnail mode: grid of selectable previews of all images
* Ability to cache thumbnails for fast re-loading
* Basic support for multi-frame images
* Load all frames from GIF files and play GIF animations
* Display image information in status bar


Screenshots
-----------

**Image mode:**

TODO

**Thumbnail mode:**

TODO


Dependencies
------------

swiv requires the following software to be installed:

  * Imlib2
  * cairo
  * fontconfig
  * pango
  * libwayland
  * xkbcommon
  * giflib (optional, disabled with `HAVE_GIFLIB=0`)
  * libexif (optional, disabled with `HAVE_LIBEXIF=0`)

Please make sure to install the corresponding development packages in case that
you want to build swiv on a distribution with separate runtime and development
packages (e.g. \*-dev on Debian).


Building
--------

swiv is built using the commands:

    $ make
    # make install

Please note, that the latter one requires root privileges.
By default, swiv is installed using the prefix "/usr/local", so the full path
of the executable will be "/usr/local/bin/swiv".

You can install swiv into a directory of your choice by changing the second
command to:

    # make PREFIX="/your/dir" install

The build-time specific settings of swiv can be found in the file *config.h*.
Please check and change them, so that they fit your needs.
If the file *config.h* does not already exist, then you have to create it with
the following command:

    $ make config.h

If you want to use your old sxiv *config.h*, the key and mouse mappings will
need to be changed. Here's some help on modifying the *config.h* for swiv, it
will not be easy if it's heavily modified.

A bunch of macros should be defined in *config.h* as the equivalents in X, like
`ControlMask` and `None` (look at the example *config.def.h*).

The keys must be changed from `XK_*` to `XKB_KEY_*`, this sed command
should work:

    $ sed 's/XK_/XKB_KEY_/g'

The mouse button and scroll mappings are separated and the buttons use
`linux/input-event-codes.h` constants e.g `BTN_LEFT` instead of `1` or `Button1`.

The scroll mappings have an axis and direction:

|-----------------------------------|-----------|--------------------|
| axis                              | direction | absolute direction |
|-----------------------------------|-----------|--------------------|
| WL_POINTER_AXIS_VERTICAL_SCROLL   | +1        | down               |
|-----------------------------------|-----------|--------------------|
| WL_POINTER_AXIS_VERTICAL_SCROLL   | -1        | up                 |
|-----------------------------------|-----------|--------------------|
| WL_POINTER_AXIS_HORIZONTAL_SCROLL | +1        | right              |
|-----------------------------------|-----------|--------------------|
| WL_POINTER_AXIS_HORIZONTAL_SCROLL | -1        | left               |
|-----------------------------------|-----------|--------------------|

Take a look at the example *config.def.h* before making changes.

Usage
-----

Please see the [man page](./swiv.1) for information on how to use swiv.

Download & Changelog
--------------------

TODO
