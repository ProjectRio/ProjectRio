#!/bin/bash -e
# linux-env.sh

if [[ $(env | grep -i wayland) ]]; then
    # wxWidgets 3.14 is GTK3, which seemingly has an issue or two when
    # running under Wayland. Explicitly setting this for Slippi avoids
    # those issues.
    export GDK_BACKEND=x11

    # Disable Webkit compositing on Wayland cause it breaks stuff
    export WEBKIT_DISABLE_COMPOSITING_MODE=1
fi 