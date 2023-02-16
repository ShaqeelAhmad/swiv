**Simple Wayland Image Viewer**

A fork of [sxiv](https://github.com/xyb3rt/sxiv) for wayland.

Font's are configured with the `-F` command line option and the syntax for it
follows
[pango_font_description_from_string](https://docs.gtk.org/Pango/type_func.FontDescription.from_string.html#description)
there are also `-B` and `-C` options for setting the background and foreground
color respectively.

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
need to be changed. For keys, the key must be changed from `XK_*` to
`XKB_KEY_*`, this sed command might work:

    $ sed 's/XK_/XKB_KEY_/g'

and the mouse mappings have been separated to scroll and button click mappings.
It isn't easy to automate this part so take a look at the example config.def.h
and change it manually.

Usage
-----

Please see the [man page](./swiv.1) for information on how to use swiv.

Download & Changelog
--------------------

TODO
