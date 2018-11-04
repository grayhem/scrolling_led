"""
this module decides what kind of horrible content to send to the sign.
hit control-c to cancel the current program.
"""


import unicodedata

from scrolling_led import master
from scrolling_led import text



# we're actually gonna join lines together
JOINER = "    "

# how many pixels wide is the font?
# the slave adds one pixel between characters.
CHARACTER_WIDTH = 5 + 1

# how many pixels/ characters wide is the sign?
DISPLAY_PIXELS = 180
DISPLAY_CHARACTERS = DISPLAY_PIXELS // CHARACTER_WIDTH

# how many characters can be in a line? hint it's twice the display
LINE_CHARACTERS = DISPLAY_CHARACTERS * 2


class Sender(object):
    """
    manages state blah blah blah
    """

    def __init__(self, **kwargs):
        self.joiner = JOINER
        self.line_characters = LINE_CHARACTERS
        self.display_characters = DISPLAY_CHARACTERS
        self.long_ok = True
        self.verbose = True

        for key, value in kwargs.items():
            self.__setattr__(key, value)

        self.line_generator = text.random_text(long_ok=self.long_ok, verbose=self.verbose)

        self.display_buffer = " " * self.display_characters

        self.driver = master.Master()
        self.driver.just_connect()



    def fill_buffer(self):
        """
        make sure the display buffer has enough stuff left in it to refresh the sign
        """
        while len(self.display_buffer) < self.line_characters:
            try:
                next_line = self.line_generator.__next__()
            # which might mean refreshing the line generator. when we do so, make sure to clear the
            # display.
            except StopIteration:
                next_line = " " * self.display_characters
                self.line_generator = text.random_text(long_ok=self.long_ok, verbose=self.verbose)
            else:
                next_line = self.joiner + unicodedata.normalize('NFKD', next_line)
            self.display_buffer += next_line


    def run(self):
        """
        pretty simple
        """
        while True:
            try:
                self.fill_buffer()
                this_line = self.display_buffer[:self.line_characters]
                self.driver.send_it(this_line)
                strip = self.line_characters-self.display_characters
                self.display_buffer = self.display_buffer[strip:]
            except KeyboardInterrupt:
                choice = input('(r)efresh text source or (q)uit? >>> ')
                if choice in ['r', 'R']:
                    self.display_buffer = " " * self.display_characters
                    self.line_generator = text.random_text(
                        long_ok=self.long_ok,
                        verbose=self.verbose)
                elif choice in ['q', 'Q']:
                    break
