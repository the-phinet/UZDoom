import os.path

application = defines.get("app", "/System/Applications/TextEdit.app")  # noqa: F821
appname = os.path.basename(application)
files = [application]
symlinks = {"Applications": "/Applications"}
badge_icon = os.path.join(application, "Contents/Resources/zdoom.icns")
icon_locations = {appname: (140, 120), "Applications": (500, 120)}
background = "builtin-arrow"
show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
sidebar_width = 180
window_rect = ((100, 100), (640, 280))
default_view = "icon-view"
include_icon_view_settings = "auto"
include_list_view_settings = "auto"
