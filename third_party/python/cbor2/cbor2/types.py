from collections import Mapping


class CBORTag(object):
    """
    Represents a CBOR semantic tag.

    :param int tag: tag number
    :param value: encapsulated value (any object)
    """

    __slots__ = 'tag', 'value'

    def __init__(self, tag, value):
        self.tag = tag
        self.value = value

    def __eq__(self, other):
        if isinstance(other, CBORTag):
            return self.tag == other.tag and self.value == other.value
        return NotImplemented

    def __repr__(self):
        return 'CBORTag({self.tag}, {self.value!r})'.format(self=self)


class CBORSimpleValue(object):
    """
    Represents a CBOR "simple value".

    :param int value: the value (0-255)
    """

    __slots__ = 'value'

    def __init__(self, value):
        if value < 0 or value > 255:
            raise TypeError('simple value too big')
        self.value = value

    def __eq__(self, other):
        if isinstance(other, CBORSimpleValue):
            return self.value == other.value
        elif isinstance(other, int):
            return self.value == other
        return NotImplemented

    def __repr__(self):
        return 'CBORSimpleValue({self.value})'.format(self=self)


class FrozenDict(Mapping):
    """
    A hashable, immutable mapping type.

    The arguments to ``FrozenDict`` are processed just like those to ``dict``.
    """

    def __init__(self, *args, **kwargs):
        self._d = dict(*args, **kwargs)
        self._hash = None

    def __iter__(self):
        return iter(self._d)

    def __len__(self):
        return len(self._d)

    def __getitem__(self, key):
        return self._d[key]

    def __repr__(self):
        return "%s(%s)" % (self.__class__.__name__, self._d)

    def __hash__(self):
        if self._hash is None:
            self._hash = hash((frozenset(self), frozenset(self.values())))
        return self._hash


class UndefinedType(object):
    __slots__ = ()


#: Represents the "undefined" value.
undefined = UndefinedType()
break_marker = object()
