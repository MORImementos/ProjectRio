"""
Module for interacting with the emulated machine's registers
"""


def read_gpr(regnum: int, /) -> int:
    """
    Reads value at given register.

    :param regnum: register number to read from
    :return: value as integer
    """


def read_fpr(regnum: int, /) -> float:
    """
    Reads value at given float register.
    
    :param regnum: register number to read from
    :return: value as float
    """


def write_gpr(regnum: int, value: int, /) -> None:
    """
    Writes value at given register number.

    :param regnum: register number to read from
    :value: value as integer
    """


def write_fpr(regnum: int, value: float, /) -> None:
    """
    Writes value at given float register.

    :param regnum: register number to read from
    :value: value as float
    """


def read_ctr() -> int:
    """
    Reads CTR
    """


def read_lr() -> int:
    """
    Reads LR
    """


def read_cr() -> int:
    """
    Reads CR
    """


def read_pc() -> int:
    """
    Reads PC
    """


def read_npc() -> int:
    """
    Reads NPC
    """


def write_cr(value: int) -> None:
    """
    Writes value to CR
    """
