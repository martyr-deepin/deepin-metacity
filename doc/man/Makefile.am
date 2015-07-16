man_MANS = metacity.1 metacity-theme-viewer.1 \
	   metacity-window-demo.1 metacity-message.1

EXTRA_DIST = $(man_MANS)

-include $(top_srcdir)/git.mk

install-data-hook:
	cd $(DESTDIR) && find ./usr/share/man -type f -a -name '*metacity*' | awk '{a=$$0; gsub("metacity", "deepin-metacity", $$0); print "mv" ,a,$$0}' | sh -
