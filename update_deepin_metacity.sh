#!/bin/bash

# migrate code from metacity to deepin-metacity
appname="$(basename $0)"

grep_ignore_files="${appname}\|README\|NEWS\|Makefile.am\|\.git\|\./po\|\./debian"

echo "==> show gsettings path with prefix 'org.gnome'"
find . -type f | grep -v "${grep_ignore_files}" | xargs grep -P 'org.gnome.[^A-Z]'

echo "==> replace gsettings path"
for f in $(find . -type f | grep -v "${grep_ignore_files}" | xargs grep -l -P 'org.gnome.[^A-Z]'); do
  echo "  -> ${f}"
  sed -e 's,org.gnome.,com.deepin.wrap.gnome.,' \
      -e 's,/org/gnome/,/com/deepin/wrap/gnome/,' -i "${f}"
done
