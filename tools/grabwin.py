import sys
from Xlib import display, X
from PIL import Image
d = display.Display()
w = d.create_resource_object("window", int(sys.argv[1], 16))
g = w.get_geometry()
raw = w.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX").save(sys.argv[2])
print("saved", sys.argv[2], g.width, g.height)
