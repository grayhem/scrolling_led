"""
how do we load text?
these will probably be generators that yield lines from text files.
"""

from pathlib import Path

import numpy as np


TEXT_PATH = Path("/home/graham/python/led_sign/text/")
POETRY = lambda x: x.stem.endswith("_poetry")
QUIPS = lambda x: x.stem.endswith("_quips")
LONG = lambda x: x.stem.endswith("_long")
PROSE = lambda x: x.stem.endswith("_prose")
INSTRUCTIONS = lambda x: x.stem.endswith("_instructions")


def random_text(long_ok=False, verbose=False):
    """
    yield text from a file chosen at random. even if you don't want a long file, we might give ya
    one anyway.
    """
    filepaths = list(TEXT_PATH.glob("*.txt"))
    this_path = np.random.choice(filepaths)
    if verbose:
        print(this_path.stem)

    if POETRY(this_path):
        return lines(this_path)
    elif QUIPS(this_path):
        return random_lines(this_path)
    elif PROSE(this_path):

        return sentences(this_path)
    elif INSTRUCTIONS(this_path):
        if np.random.choice([True, False]):
            return random_sentences(this_path)
        else:
            return sentences(this_path)
    elif LONG(this_path):
        if long_ok:
            return sentences(this_path)
        else:
            if np.random.choice([True, False]):
                return sentences(this_path)
            else:
                return random_text(long_ok=True)
            return random_text(long_ok=False)
    else:
        raise IOError("i don't know what to do with {}! help!".format(this_path.name))


def random_lines(filepath):
    """
    open the file at the end of the path and yield LINES at random
    """
    stuff = list(lines(filepath))
    np.random.shuffle(stuff)

    yield from stuff

def strip_newlines(line):
    """
    return the string with any newlines removed and replaced with a space
    """
    parts = line.split("\n")
    line = "    ".join(parts)
    return line


def sentences(filepath):
    """
    i think for prose we should probably let the sign clear between sentences.
    """
    with filepath.open('r') as thefile:
        whole_thing = thefile.read()
    whole_thing = [l + "." for l in whole_thing.split(". ") if len(l) > 1]
    yield from whole_thing


def random_sentences(filepath):
    """
    you guessed it
    (this is great with sets of instructions)
    """
    stuff = list(sentences(filepath))

    np.random.shuffle(stuff)

    yield from stuff


def lines(filepath):
    """
    for poetry, probably let the sign clear on newlines.
    """
    with filepath.open('r') as thefile:
        stuff = [l[:-1] for l in thefile.readlines() if len(l) > 1]
    yield from stuff


def append_ellipsis(gen):
    """
    generators on generators. add ellipsis
    """
    for thing in gen:
        yield "{}...".format(thing)


def random_counter(high, count=100):
    """
    yield random numbers
    """
    numbers = np.random.permutation(high)[:count]
    yield from append_ellipsis(numbers)


def reliable_counter(high):
    """
    count straight from 0 to high
    """
    numbers = np.arange(high)
    yield from append_ellipsis(numbers)


def unreliable_counter(count, randomness=10):
    """
    count up, down, around...
    the higher `randomness` is, the more unreliable. make it smaller than `count`, preferably by
    about an order of magnitude.
    i like this one.
    """
    # MAKE ME LESS RANDOM (sequences of random numbers make this too messy)
    segment_length = count // randomness
    numbers = np.arange(count)
    for _ in range(randomness):
        choice = np.random.randint(6)
        start = np.random.randint(count - segment_length)
        stop = start + segment_length
        if choice == 0:
            segment = np.arange(segment_length )+ np.random.randint(count)
        elif choice == 1:
            segment = numbers[stop:start:-1]
        elif choice == 2:
            segment = np.arange(segment_length)[::-1] + np.random.randint(count)
        elif choice == 4:
            segment = np.zeros(segment_length) + np.random.randint(count)
        numbers[start: stop] = segment
    yield from append_ellipsis(numbers)


def littering_and(iterable, max_repeats=3, probability=0.05, max_sequence_length=5):
    """
    littering and, littering and, littering and, littering and,
    """

    for thing in iterable:
        if np.random.random() < probability:
            # read the next few and put into memory
            memory = [thing]
            repeats = np.random.randint(1, max_repeats)
            sequence_length = np.random.randint(1, max_sequence_length)
            try:
                for _ in range(sequence_length):
                    memory.append(iterable.__next__())
            except StopIteration:
                # if we don't catch this we might never see the last thing in the iterable
                pass
            memory = memory * repeats
            for this in memory:
                yield this
        else:
            yield thing


def reflect(iterable, max_sequence_length=5, probability=0.05):
    """
    abcdedcbabcdefghijk
    """

    for thing in iterable:
        if np.random.random() < probability:
            memory = [thing]
            sequence_length = np.random.randint(1, max_sequence_length)
            try:
                for _ in range(sequence_length):
                    memory.append(iterable.__next__())
            except StopIteration:
                pass
            # etc etc
            memory = memory + memory[-2::-1] + memory[1:]
            yield from memory
        else:
            yield thing





