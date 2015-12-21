NULL =

lib_LTLIBRARIES = libdeepin-metacity-private.la

SUBDIRS = tools themes

AM_CPPFLAGS = \
	-DMETACITY_LIBEXECDIR=\"$(libexecdir)\" \
	-DHOST_ALIAS=\"@HOST_ALIAS@\" \
	-DMETACITY_LOCALEDIR=\""$(localedir)"\" \
	-DMETACITY_PKGDATADIR=\"$(pkgdatadir)\" \
	-DMETACITY_DATADIR=\"$(datadir)\" \
	-DG_LOG_DOMAIN=\"deepin_metacity\" \
	-DSN_API_NOT_YET_FROZEN=1 \
	-I$(srcdir)/include \
	@METACITY_CFLAGS@ \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

deepin_metacity_built_sources = \
	$(deepin_dbus_wm_built_sources)	\
	$(NULL)

deepin_metacity_SOURCES= 				\
	core/main.c				\
	include/main.h

# by setting libdeepin_metacity_private_la_CFLAGS, the files shared with
# deepin_metacity proper will be compiled with different names.
libdeepin_metacity_private_la_CFLAGS =
libdeepin_metacity_private_la_SOURCES=			\
	core/async-getprop.c			\
	core/async-getprop.h			\
	core/atomnames.h                \
	core/bell.c				\
	core/bell.h				\
	core/boxes.c				\
	include/boxes.h				\
	core/deepin-message-hub.c  		\
	include/deepin-message-hub.h    \
	compositor/compositor.c			\
	compositor/compositor-private.h		\
	compositor/compositor-xrender.c		\
	compositor/compositor-xrender.h		\
	include/compositor.h			\
	core/above-tab-keycode.c		\
	core/constraints.c			\
	core/constraints.h			\
	core/core.c					\
	core/delete.c				\
	core/display.c				\
	core/display-private.h		\
	include/display.h			\
	ui/draw-workspace.c			\
	ui/draw-workspace.h			\
	core/edge-resistance.c			\
	core/edge-resistance.h			\
	core/effects.c				\
	core/effects.h				\
	core/errors.c				\
	include/errors.h			\
	core/eventqueue.c			\
	core/eventqueue.h			\
	core/frame.c				\
	core/frame-private.h		\
	include/frame.h				\
	ui/gradient.c				\
	ui/gradient.h				\
	core/group-private.h		\
	core/group-props.c			\
	core/group-props.h			\
	core/group.c				\
	core/group.h				\
	core/iconcache.c			\
	core/iconcache.h			\
	core/keybindings.c			\
	core/keybindings.h			\
	core/deepin-keybindings.c   \
	core/deepin-keybindings.h   \
	core/deepin-timeline.c	    \
	include/deepin-timeline.h	\
	core/main-private.c			\
	include/main.h				\
	core/metacity-Xatomtype.h	\
	core/place.c				\
	core/place.h				\
	core/prefs.c				\
	include/prefs.h				\
	core/screen.c				\
	core/screen-private.h		\
	include/screen.h			\
	include/types.h             \
	core/session.c				\
	core/session.h				\
	core/stack.c				\
	core/stack.h				\
	core/util.c					\
	include/util.h				\
	core/window-props.c			\
	core/window-props.h			\
	core/window.c				\
	core/window-private.h		\
	include/window.h			\
	core/workspace.c			\
	core/workspace.h			\
	core/xprops.c				\
	core/deepin-dbus-service.c  	\
	include/deepin-dbus-service.h  	\
	include/xprops.h			\
	include/common.h			\
	include/core.h				\
	include/ui.h				\
	ui/fixedtip.c				\
	ui/fixedtip.h				\
	ui/frames.c				\
	ui/frames.h				\
	ui/menu.c				\
	ui/menu.h				\
	ui/metaaccellabel.c			\
	ui/metaaccellabel.h			\
	ui/preview-widget.c         \
	ui/preview-widget.h         \
	ui/resizepopup.c			\
	include/resizepopup.h		\
	ui/tabpopup.c				\
	include/tabpopup.h			\
	ui/select-image.c			\
	ui/select-image.h			\
	ui/deepin-tabpopup.c		\
	include/deepin-tabpopup.h	\
	ui/deepin-ease.c			\
	include/deepin-ease.h		\
	ui/deepin-tab-widget.c		\
	include/deepin-tab-widget.h	\
	ui/deepin-fixed.c			\
	include/deepin-fixed.h		\
	ui/deepin-design.c			\
	include/deepin-design.h				\
	ui/deepin-stackblur.c				\
	include/deepin-stackblur.h			\
	ui/deepin-switch-previewer.c		\
	include/deepin-switch-previewer.h	\
	ui/deepin-cloned-widget.c			\
	include/deepin-cloned-widget.h		\
	ui/deepin-wm-background.c			\
	include/deepin-wm-background.h		\
	ui/deepin-desktop-background.c			\
	include/deepin-desktop-background.h		\
	ui/deepin-background-cache.c		\
	include/deepin-background-cache.h	\
	ui/deepin-shadow-workspace.c		\
	include/deepin-shadow-workspace.h	\
	ui/deepin-window-surface-manager.c	\
	include/deepin-window-surface-manager.h	\
	ui/deepin-workspace-adder.c 		\
	include/deepin-workspace-adder.h 	\
	ui/deepin-name-entry.c      \
	include/deepin-name-entry.h \
	ui/deepin-workspace-indicator.c			\
	include/deepin-workspace-indicator.h	\
	ui/select-workspace.c		\
	ui/select-workspace.h		\
	ui/tile-preview.c			\
	include/tile-preview.h		\
	ui/theme-parser.c			\
	ui/theme-parser.h			\
	ui/theme.c				\
	ui/theme.h				\
	ui/ui.c

nodist_libdeepin_metacity_private_la_SOURCES = $(deepin_metacity_built_sources)

libdeepin_metacity_private_la_LDFLAGS = -no-undefined -version-info 3:0:0 $(WARN_LDFLAGS) $(AM_LDFLAGS)
libdeepin_metacity_private_la_LIBADD  = @METACITY_LIBS@

libdeepin_metacityincludedir = $(includedir)/deepin-metacity/metacity-private

libdeepin_metacityinclude_HEADERS =			\
	include/boxes.h				\
	ui/gradient.h				\
	include/util.h				\
	include/common.h			\
	ui/preview-widget.h			\
	ui/theme-parser.h			\
	ui/theme.h

deepin_metacity_theme_viewer_SOURCES=  \
	ui/theme-viewer.c

gsettings_SCHEMAS = org.gnome.metacity.gschema.xml
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings/
convert_DATA = metacity-schemas.convert

bin_PROGRAMS=deepin-metacity deepin-metacity-theme-viewer

deepin_metacity_LDADD=@METACITY_LIBS@ libdeepin-metacity-private.la
deepin_metacity_theme_viewer_LDADD= @METACITY_LIBS@ libdeepin-metacity-private.la

testboxes_SOURCES=include/util.h core/util.c include/boxes.h core/boxes.c core/testboxes.c
testgradient_SOURCES=ui/gradient.h ui/gradient.c ui/testgradient.c
testasyncgetprop_SOURCES=core/async-getprop.h core/async-getprop.c core/testasyncgetprop.c

noinst_PROGRAMS=testboxes testgradient testasyncgetprop

testboxes_LDADD= @METACITY_LIBS@
testgradient_LDADD= @METACITY_LIBS@
testasyncgetprop_LDADD= @METACITY_LIBS@

@INTLTOOL_DESKTOP_RULE@

desktopfilesdir=$(datadir)/applications
desktopfiles_in_files=metacity.desktop.in
desktopfiles_files=$(desktopfiles_in_files:.desktop.in=.desktop)
desktopfiles_DATA = $(desktopfiles_files)

wmpropertiesdir=$(datadir)/gnome/wm-properties
wmproperties_in_files=metacity-wm.desktop.in
wmproperties_files=$(wmproperties_in_files:.desktop.in=.desktop)
wmproperties_DATA = $(wmproperties_files)

xmldir       = @GNOME_KEYBINDINGS_KEYSDIR@
xml_in_files = \
	50-metacity-navigation.xml.in	\
	50-metacity-system.xml.in	\
	50-metacity-windows.xml.in

xml_DATA     = $(xml_in_files:.xml.in=.xml)

@INTLTOOL_SCHEMAS_RULE@

CLEANFILES = \
	$(deepin_metacity_built_sources)			\
	metacity.desktop metacity-wm.desktop org.gnome.metacity.gschema.xml 50-metacity-launchers.xml 50-metacity-navigation.xml 50-metacity-screenshot.xml 50-metacity-system.xml 50-metacity-windows.xml

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = libmetacity-private.pc

stylesdir = $(pkgdatadir)
styles_DATA = deepin-wm.css texture.png close.png

EXTRA_DIST=$(desktopfiles_files)	 	\
	$(wmproperties_files)			\
	$(IMAGES)		 		\
	$(desktopfiles_in_files)		\
	$(wmproperties_in_files)		\
	metacity-schemas.convert		\
	org.gnome.metacity.gschema.xml.in	\
	$(xml_in_files)				\
	libmetacity-private.pc.in \
	deepin-wm.css \
	close.png \
	texture.png \
	com.deepin.wm.xml

BUILT_SOURCES =					\
	$(deepin_metacity_built_sources)

-include $(top_srcdir)/git.mk

deepin_dbus_wm_built_sources = deepin-dbus-wm.c deepin-dbus-wm.h

$(deepin_dbus_wm_built_sources) : Makefile.am com.deepin.wm.xml
	$(AM_V_GEN)gdbus-codegen							\
		--interface-prefix com.deepin					\
		--c-namespace DeepinDBus						\
		--generate-c-code deepin-dbus-wm 				\
		$(srcdir)/com.deepin.wm.xml

# add prefix 'deepin-'
install-data-hook:
	cd $(DESTDIR) && find ./ -type f -a -name '*metacity*' |grep -v gschema.xml | grep -v deepin-metacity | awk '{a=$$0; gsub("metacity", "deepin-metacity", $$0); print "mv" ,a,$$0}' | sh -
	mv $(DESTDIR)$(gsettingsschemadir)/org.gnome.metacity.gschema.xml $(DESTDIR)$(gsettingsschemadir)/com.deepin.wrap.gnome.metacity.gschema.xml

