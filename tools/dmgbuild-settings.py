import os.path

application = defines.get("app", "/System/Applications/TextEdit.app")  # noqa: F821
appname = os.path.basename(application)
files = [application]
symlinks = {"Applications": "/Applications"}
badge_icon = os.path.join(application, "Contents/Resources/zdoom.icns")
icon_locations = {appname: (140, 120), "Applications": (500, 120)}
window_rect = ((100, 100), (640, 280))
