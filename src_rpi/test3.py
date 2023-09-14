import sys
import gpiozero
from time import sleep

import Adafruit_GPIO.SPI as SPI
import Adafruit_SSD1306

from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

# Raspberry Pi pin configuration:
RST = 24
# Note the following are only used with SPI:
DC = 23
SPI_PORT = 0
SPI_DEVICE = 0
disp = Adafruit_SSD1306.SSD1306_128_64(rst=RST)
# Initialize library.
disp.begin()

# Clear display.
disp.clear()
disp.display()

width = disp.width
height = disp.height
image = Image.new('1', (width, height))
draw = ImageDraw.Draw(image)
x = 0
top = 0

font = ImageFont.load_default()
draw.text((x, top),    'Hello',  font=font, fill=255)
draw.text((x, top+20), 'World!', font=font, fill=255)

disp.image(image)
disp.display()

class CBeam:
    def __init__(self):
        self.stpcm = int(1)
        self.pin_pulse = 17
        self.pin_dir = 27
        self.dt = 0.002

        self.od_pulse = gpiozero.OutputDevice(self.pin_pulse, True, False, None)
        self.od_dir = gpiozero.OutputDevice(self.pin_dir, True, False, None)

    def mymove(self, dist):

        if dist < 0:
            self.od_dir.on()
            sign = -1
        else:
            self.od_dir.off()
            sign = 1
        sleep(self.dt)
        steps = int(sign * dist * self.stpcm)
        for i in range(steps):
            pos = sign * i / self.stpcm * 10
            self.od_pulse.on()
            sleep(self.dt)
            self.od_pulse.off()

        disp.clear()
        disp.display()
        image = Image.new('1', (width, height))
        draw = ImageDraw.Draw(image)
        draw.text((x, top),    f'Pos: {pos:+.3f} mm',  font=font, fill=255)
        disp.image(image)
        disp.display()


def main(args=None):
    if args is None:
        args = sys.argv[1:]

    dist = float(args[0])

    a = CBeam()
    a.mymove(dist)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
