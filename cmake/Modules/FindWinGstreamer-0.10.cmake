#  - Try to find Gstreamer-0.10
#
#  GSTREAMER_INCLUDE_DIR - the Gstreamer-0.10 include directory
#  GSTREAMER_LIBRARY - the Gstreamer-0.10 library

set(GSTREAMER_INCLUDE_DIR
    $ENV{SDL_GSTREAMER_DIR}/include/gstreamer-0.10
    $ENV{SDL_GSTREAMER_DIR}/include/libxml2
    $ENV{SDL_GSTREAMER_DIR}/include/glib-2.0
    $ENV{SDL_GSTREAMER_DIR}/lib/glib-2.0/include
)
set(GSTREAMER_LIBRARY
    $ENV{SDL_GSTREAMER_DIR}/lib/gstreamer-0.10.lib
    $ENV{SDL_GSTREAMER_DIR}/lib/glib-2.0.lib
    $ENV{SDL_GSTREAMER_DIR}/lib/gobject-2.0.lib
)
