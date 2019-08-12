# Note: plugindir is set in configure

##############################################################################
# TODO: change libstillreplace.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libstillreplace.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

## Plugin 1

# sources used to compile this plug-in
libstillreplace_la_SOURCES = gststillreplacefilter.c gststillreplacefilter.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libstillreplace_la_CFLAGS = $(GST_CFLAGS)
libstillreplace_la_LIBADD = $(GST_LIBS) -lgstvideo-1.0
libstillreplace_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libstillreplace_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gststillreplacefilter.h
