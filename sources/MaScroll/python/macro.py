import abc
import collections
import collections.abc
import copy
import dataclasses
import inspect
import random
import string
import typing


_T = typing.TypeVar('_T')


@dataclasses.dataclass
class Value(typing.Generic[_T], metaclass=abc.ABCMeta):
    @abc.abstractmethod
    def __str__(self) -> str:
        pass

    @abc.abstractmethod
    def contains(self, other: 'Value') -> bool:
        pass

    @abc.abstractmethod
    def concrete(self) -> typing.Optional[_T]:
        pass

    @abc.abstractmethod
    def get_var_decl(self, name: str) -> tuple[str, str]:
        pass

    @staticmethod
    @abc.abstractmethod
    def get_param_decl(name: str) -> tuple[str, str]:
        pass

    @staticmethod
    @abc.abstractmethod
    def abstract(*values: _T) -> 'Value[_T]':
        pass

    @staticmethod
    @abc.abstractmethod
    def rand_literal() -> 'Value[_T]':
        pass


@dataclasses.dataclass
class Boolean(Value[bool]):
    value: typing.Optional[bool] = None

    def __bool__(self) -> bool:
        # Avoid mistakes from rich comparison return values.
        raise NotImplementedError()

    def __str__(self) -> str:
        value = self.concrete()
        assert value is not None
        return "True" if value else "False"

    def __and__(self, other: 'Boolean') -> 'Boolean':
        if self.value == False or other.value == False:
            return Boolean.abstract(False)
        if self.value == True and other.value == True:
            return Boolean.abstract(True)
        return Boolean()

    def __or__(self, other: 'Boolean') -> 'Boolean':
        if self.value == True or other.value == True:
            return Boolean.abstract(True)
        if self.value == False and other.value == False:
            return Boolean.abstract(False)
        return Boolean()

    def __invert__(self) -> 'Boolean':
        return Boolean(value=not self.value if self.value is not None else None)

    def contains(self, other: Value) -> bool:
        if not isinstance(other, Boolean):
            return False
        return self.value is None or self.value == other.value

    def concrete(self) -> typing.Optional[bool]:
        return self.value

    def get_var_decl(self, name: str) -> tuple[str, str]:
        return name, 'Boolean'

    @staticmethod
    def get_param_decl(name: str) -> tuple[str, str]:
        return name, 'Boolean'

    @staticmethod
    def abstract(*values: bool) -> 'Boolean':
        num_true = values.count(True)
        if num_true == 0:
            return Boolean(value=False)
        if num_true == len(values):
            return Boolean(value=True)
        return Boolean()

    @staticmethod
    def rand_literal() -> 'Boolean':
        value = random.choice([True, False])
        return Boolean.abstract(value)


@dataclasses.dataclass
class Integer(Value[int]):
    INT_MIN: typing.ClassVar[int] = -(1 << 63)
    INT_MAX: typing.ClassVar[int] = (1 << 63) - 1

    value_min: int = INT_MIN
    value_max: int = INT_MAX

    def __post_init__(self):
        assert self.value_min <= self.value_max
        # Take two's complement truncated to 64 bits.
        a = self.value_min & ((1 << 64) - 1)
        if a & (1 << 63) != 0:
            a = a - (1 << 64)
        b = self.value_max & ((1 << 64) - 1)
        if b & (1 << 63) != 0:
            b = b - (1 << 64)
        if a > b:
            # We have wraparound.
            self.value_min = Integer.INT_MIN
            self.value_max = Integer.INT_MAX
        else:
            self.value_min = a
            self.value_max = b

    def __str__(self) -> str:
        value = self.concrete()
        assert value is not None
        return str(value)

    def __add__(self, other: 'Integer') -> 'Integer':
        return Integer(
            value_min=self.value_min + other.value_min,
            value_max=self.value_max + other.value_max,
        )

    def __sub__(self, other: 'Integer') -> 'Integer':
        return Integer(
            value_min=self.value_min - other.value_max,
            value_max=self.value_max - other.value_min,
        )

    def __mul__(self, other: 'Integer') -> 'Integer':
        return Integer.abstract(
            self.value_min * other.value_min,
            self.value_min * other.value_max,
            self.value_max * other.value_min,
            self.value_max * other.value_max,
        )

    def __floordiv__(self, other: 'Integer') -> 'Integer':
        assert not other.contains(Integer.abstract(0))
        return Integer.abstract(
            _c_int_div(self.value_min, other.value_min),
            _c_int_div(self.value_min, other.value_max),
            _c_int_div(self.value_max, other.value_min),
            _c_int_div(self.value_max, other.value_max),
        )

    def __and__(self, other: 'Integer') -> 'Integer':
        return Integer()

    def __or__(self, other: 'Integer') -> 'Integer':
        return Integer()

    def __neg__(self) -> 'Integer':
        return Integer(
            value_min=-self.value_max,
            value_max=-self.value_min,
        )

    def __invert__(self) -> 'Integer':
        return Integer(
            value_min=~self.value_max,
            value_max=~self.value_min,
        )

    def __lt__(self, other: 'Integer') -> Boolean:
        if self.value_max < other.value_min:
            return Boolean.abstract(True)
        if self.value_min >= other.value_max:
            return Boolean.abstract(False)
        return Boolean()

    def __le__(self, other: 'Integer') -> Boolean:
        if self.value_max <= other.value_min:
            return Boolean.abstract(True)
        if self.value_min > other.value_max:
            return Boolean.abstract(False)
        return Boolean()

    def __gt__(self, other: 'Integer') -> Boolean:
        if self.value_min > other.value_max:
            return Boolean.abstract(True)
        if self.value_max <= other.value_min:
            return Boolean.abstract(False)
        return Boolean()

    def __ge__(self, other: 'Integer') -> Boolean:
        if self.value_min >= other.value_max:
            return Boolean.abstract(True)
        if self.value_max < other.value_min:
            return Boolean.abstract(False)
        return Boolean()

    def __eq__(self, other: 'Integer') -> Boolean: # type: ignore [override]
        self_conc = self.concrete()
        if self_conc is not None:
            other_conc = other.concrete()
            if other_conc is not None:
                return Boolean.abstract(self_conc == other_conc)
        if self.value_max < other.value_min or self.value_min > other.value_max:
            return Boolean.abstract(False)
        return Boolean()

    def __ne__(self, other: 'Integer') -> Boolean: # type: ignore [override]
        self_conc = self.concrete()
        if self_conc is not None:
            other_conc = other.concrete()
            if other_conc is not None:
                return Boolean.abstract(self_conc != other_conc)
        if self.value_max < other.value_min or self.value_min > other.value_max:
            return Boolean.abstract(True)
        return Boolean()

    def contains(self, other: Value) -> bool:
        if not isinstance(other, Integer):
            return False
        return self.value_min <= other.value_min and self.value_max >= other.value_max

    def concrete(self) -> typing.Optional[int]:
        return self.value_min if self.value_min == self.value_max else None

    def get_var_decl(self, name: str) -> tuple[str, str]:
        return name, 'Integer'

    @staticmethod
    def get_param_decl(name: str) -> tuple[str, str]:
        return name, 'Integer'

    @staticmethod
    def abstract(*values: int) -> 'Integer':
        return Integer(value_min=min(values), value_max=max(values))

    @staticmethod
    def rand_literal() -> 'Integer':
        value = round(random.triangular(Integer.INT_MIN, Integer.INT_MAX, 0))
        return Integer.abstract(value)


@dataclasses.dataclass
class IntegerArray(Value[list[int]]):
    length: Integer = dataclasses.field(
        default_factory=lambda: Integer(value_min=0))
    elements: collections.defaultdict[int, Integer] = dataclasses.field(
        default_factory=lambda: collections.defaultdict(
            lambda: Integer.abstract(0)))

    def __str__(self) -> str:
        # Array literals are not supported.
        raise NotImplementedError()

    def contains(self, other: Value) -> bool:
        if not isinstance(other, IntegerArray):
            return False
        if not self.length.contains(other.length):
            return False
        for idx, elm in self.elements.items():
            if idx in other.elements and not elm.contains(other.elements[idx]):
                return False
        return True

    def concrete(self) -> typing.Optional[list[int]]:
        length = self.length.concrete()
        if length is None:
            return None
        arr = []
        for i in range(length):
            elm = self.elements[i].concrete()
            if elm is None:
                return None
            arr.append(elm)
        return arr

    def get_var_decl(self, name: str) -> tuple[str, str]:
        length = self.length.concrete()
        assert length is not None
        return f'{name}({length})', 'Integer'

    @staticmethod
    def get_param_decl(name: str) -> tuple[str, str]:
        return f'{name}()', 'Integer'

    @staticmethod
    def abstract(*seqs: typing.Sequence[int]) -> 'IntegerArray':
        arr = IntegerArray(length=Integer.abstract(*(len(seq) for seq in seqs)))
        for i in range(arr.length.value_max):
            elm_values = []
            for seq in seqs:
                if i < len(seq):
                    elm_values.append(seq[i])
            arr.elements[i] = Integer.abstract(*elm_values)
        return arr

    @staticmethod
    def rand_literal() -> 'IntegerArray':
        length = random.randint(1, 16)
        arr = IntegerArray(length=Integer.abstract(length))
        for i in range(length):
            arr.elements[i] = Integer.rand_literal()
        return arr


@dataclasses.dataclass
class String(Value[bytes]):
    length: Integer = dataclasses.field(
        default_factory=lambda: Integer(value_min=0))
    content: collections.defaultdict[int, Integer] = dataclasses.field(
        default_factory=lambda: collections.defaultdict(
            lambda: Integer(value_min=0x00, value_max=0xff)))

    def __str__(self) -> str:
        bs = self.concrete()
        assert bs is not None
        s = ''
        for b in bs:
            if b == ord('\\'):
                s += '\\\\'
            elif b == ord('"'):
                s += '\\"'
            elif b == ord('\t'):
                s += '\\t'
            elif b == ord('\n'):
                s += '\\n'
            elif b == ord('\r'):
                s += '\\r'
            else:
                s += chr(b)
        return f'"{s}"'

    def __lt__(self, other: 'String') -> Boolean:
        self_conc = self.concrete()
        if self_conc is not None:
            other_conc = other.concrete()
            if other_conc is not None:
                return Boolean.abstract(self_conc < other_conc)
        return Boolean()

    def __le__(self, other: 'String') -> Boolean:
        self_conc = self.concrete()
        if self_conc is not None:
            other_conc = other.concrete()
            if other_conc is not None:
                return Boolean.abstract(self_conc <= other_conc)
        return Boolean()

    def __gt__(self, other: 'String') -> Boolean:
        self_conc = self.concrete()
        if self_conc is not None:
            other_conc = other.concrete()
            if other_conc is not None:
                return Boolean.abstract(self_conc > other_conc)
        return Boolean()

    def __ge__(self, other: 'String') -> Boolean:
        self_conc = self.concrete()
        if self_conc is not None:
            other_conc = other.concrete()
            if other_conc is not None:
                return Boolean.abstract(self_conc >= other_conc)
        return Boolean()

    def __eq__(self, other: 'String') -> Boolean: # type: ignore [override]
        if (self.length != other.length).concrete() == True:
            return Boolean.abstract(False)
        self_conc = self.concrete()
        if self_conc is not None:
            other_conc = other.concrete()
            if other_conc is not None:
                return Boolean.abstract(self_conc == other_conc)
        return Boolean()

    def __ne__(self, other: 'String') -> Boolean: # type: ignore [override]
        if (self.length != other.length).concrete() == True:
            return Boolean.abstract(True)
        self_conc = self.concrete()
        if self_conc is not None:
            other_conc = other.concrete()
            if other_conc is not None:
                return Boolean.abstract(self_conc != other_conc)
        return Boolean()

    def contains(self, other: Value) -> bool:
        if not isinstance(other, String):
            return False
        self_arr = IntegerArray(length=self.length, elements=self.content)
        other_arr = IntegerArray(length=other.length, elements=other.content)
        return self_arr.contains(other_arr)

    def concrete(self) -> typing.Optional[bytes]:
        bs = IntegerArray(length=self.length, elements=self.content).concrete()
        return bytes(bs) if bs is not None else None

    def get_var_decl(self, name: str) -> tuple[str, str]:
        return name, 'String'

    @staticmethod
    def get_param_decl(name: str) -> tuple[str, str]:
        return name, 'String'

    @staticmethod
    def abstract(*values: bytes) -> 'String':
        arr = IntegerArray.abstract(*values)
        s = String(length=arr.length)
        for i, value in arr.elements.items():
            s.content[i] = value
        return s

    @staticmethod
    def rand_literal() -> 'String':
        charset = (string.ascii_letters + string.digits).encode()
        length = random.randint(0, 16)
        return String.abstract(bytes(random.choices(charset, k=length)))


@dataclasses.dataclass
class Node(metaclass=abc.ABCMeta):
    inputs: tuple['ValueNode', ...]

    @abc.abstractmethod
    def __str__(self) -> str:
        pass

    def iter_tree(self) -> typing.Iterator['Node']:
        yield self
        for node in self.inputs:
            yield from node.iter_tree()


_ValueT = typing.TypeVar('_ValueT', bound=Value)


@dataclasses.dataclass
class ValueNode(Node, typing.Generic[_ValueT]):
    value: _ValueT


@dataclasses.dataclass
class Literal(ValueNode[_ValueT], typing.Generic[_ValueT]):
    inputs: tuple[()] = dataclasses.field(init=False, default=())

    def __str__(self) -> str:
        return str(self.value)


@dataclasses.dataclass(frozen=True)
class VarLoc:
    name: str
    idx: typing.Optional[ValueNode[Integer]] = None

    def __str__(self) -> str:
        s = self.name
        if self.idx is not None:
            s += f'({self.idx})'
        return s


@dataclasses.dataclass
class VarUse(ValueNode):
    inputs: tuple[()] = dataclasses.field(init=False, default=())
    var: VarLoc

    def __str__(self) -> str:
        return str(self.var)


@dataclasses.dataclass
class AssignStmt(Node):
    inputs: tuple[ValueNode]
    var: VarLoc

    def __str__(self) -> str:
        return f'{self.var} = {self.inputs[0]}'


@dataclasses.dataclass
class FunctionCall(ValueNode[_ValueT], typing.Generic[_ValueT]):
    name: str

    def __str__(self) -> str:
        return f'{self.name}({", ".join(map(str, self.inputs))})'


@dataclasses.dataclass
class Scope(Node):
    # XXX: init=False to work around the unavailability of kw_only on
    # Python < 3.10 while maintaining ergonomics (Function subclass needs
    # some mandatory init fields). The default-constructed case is the one
    # that all external code uses at the moment.
    stmts: list[Node] = dataclasses.field(init=False, default_factory=list)
    vars: dict[str, Value] = dataclasses.field(init=False, default_factory=dict)

    def __str__(self) -> str:
        lines = []
        for name, value in self.vars.items():
            decl_name, decl_type = value.get_var_decl(name)
            lines.append(f'Dim {decl_name} As {decl_type}')
        for stmt in self.stmts:
            lines.append(str(stmt))
        return '\n'.join(lines)

    def iter_tree(self) -> typing.Iterator['Node']:
        yield from super().iter_tree()
        for stmt in self.stmts:
            yield from stmt.iter_tree()


@dataclasses.dataclass
class Parameter:
    name: str
    typ: type[Value]
    is_ref: bool = False


@dataclasses.dataclass
class Function(Scope):
    inputs: tuple[()] = dataclasses.field(init=False, default=())
    name: str
    params: tuple[Parameter, ...]
    ret: type[typing.Union[Boolean, Integer, String]]

    def __str__(self) -> str:
        arg_decls = []
        for param in self.params:
            decl_name, decl_type = param.typ.get_param_decl(param.name)
            arg_decls.append(f'{"ByRef " if param.is_ref else ""}{decl_name} As {decl_type}')
        arg_decls_s = ', '.join(arg_decls)
        body = _indent_lines(super().__str__())
        s = f'Function {self.name}({arg_decls_s}) As {self.ret.get_param_decl("")[1]}\n'
        if body:
            s += f'{body}\n'
        s += 'End Function'
        return s


@dataclasses.dataclass
class IfStmt(Scope):
    inputs: tuple[ValueNode[Boolean]]

    def __str__(self) -> str:
        body = _indent_lines(super().__str__())
        s = f'If {self.inputs[0]} Then\n'
        if body:
            s += f'{body}\n'
        s += 'End If'
        return s


@dataclasses.dataclass
class WhileStmt(Scope):
    inputs: tuple[ValueNode[Boolean]]

    def __str__(self) -> str:
        body = _indent_lines(super().__str__())
        s = f'While {self.inputs[0]}\n'
        if body:
            s += f'{body}\n'
        s += 'End While'
        return s


@dataclasses.dataclass
class OpNode(ValueNode[_ValueT], typing.Generic[_ValueT], metaclass=abc.ABCMeta):
    value: _ValueT = dataclasses.field(init=False)

    def __init_subclass__(cls) -> None:
        super().__init_subclass__()
        if not inspect.isabstract(cls):
            OpNode.SUBCLASSES.append(cls)

    def __post_init__(self) -> None:
        self.value = self._compute_output()

    @abc.abstractmethod
    def _compute_output(self) -> _ValueT:
        pass

    @staticmethod
    @abc.abstractmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        pass

    @typing.final
    @staticmethod
    def rand_subtype() -> type['OpNode']:
        return random.choice(OpNode.SUBCLASSES)

    SUBCLASSES: typing.ClassVar[list[type['OpNode']]] = []


_UnOpArgT = typing.TypeVar('_UnOpArgT', bound=Value)


@dataclasses.dataclass
class UnOp(OpNode[_ValueT], typing.Generic[_ValueT, _UnOpArgT]):
    inputs: tuple[ValueNode[_UnOpArgT]]
    operator: str

    def __str__(self) -> str:
        return f'({self.operator}{self.inputs[0]})'


@dataclasses.dataclass
class UnOpNeg(UnOp[Integer, Integer]):
    operator: str = '-'

    def _compute_output(self) -> Integer:
        arg = self.inputs[0].value
        assert isinstance(arg, Integer)
        return -arg

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Integer(),)]


_BoolIntT = typing.TypeVar('_BoolIntT', Boolean, Integer)


@dataclasses.dataclass
class UnOpNot(UnOp[_BoolIntT, _BoolIntT]):
    operator: str = 'Not '

    def _compute_output(self) -> _BoolIntT:
        arg = self.inputs[0].value
        assert isinstance(arg, Boolean) or isinstance(arg, Integer)
        return ~arg

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Boolean(),), (Integer(),)]


_BinOpLhsT = typing.TypeVar('_BinOpLhsT', bound=Value)
_BinOpRhsT = typing.TypeVar('_BinOpRhsT', bound=Value)


@dataclasses.dataclass
class BinOp(OpNode[_ValueT], typing.Generic[_ValueT, _BinOpLhsT, _BinOpRhsT]):
    inputs: tuple[ValueNode[_BinOpLhsT], ValueNode[_BinOpRhsT]]
    operator: str

    def __str__(self) -> str:
        return f'({self.inputs[0]} {self.operator} {self.inputs[1]})'


@dataclasses.dataclass
class BinOpAdd(BinOp[Integer, Integer, Integer]):
    operator: str = '+'

    def _compute_output(self) -> Integer:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert isinstance(lhs, Integer) and isinstance(rhs, Integer)
        return lhs + rhs

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Integer(), Integer())]


@dataclasses.dataclass
class BinOpSub(BinOp[Integer, Integer, Integer]):
    operator: str = '-'

    def _compute_output(self) -> Integer:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert isinstance(lhs, Integer) and isinstance(rhs, Integer)
        return lhs - rhs

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Integer(), Integer())]


@dataclasses.dataclass
class BinOpMul(BinOp[Integer, Integer, Integer]):
    operator: str = '*'

    def _compute_output(self) -> Integer:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert isinstance(lhs, Integer) and isinstance(rhs, Integer)
        return lhs * rhs

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Integer(), Integer())]


@dataclasses.dataclass
class BinOpDiv(BinOp[Integer, Integer, Integer]):
    operator: str = '/'

    def _compute_output(self) -> Integer:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert isinstance(lhs, Integer) and isinstance(rhs, Integer)
        return lhs // rhs

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [
            (Integer(), Integer(value_max=-1)),
            (Integer(), Integer(value_min=1)),
        ]


@dataclasses.dataclass
class BinOpCat(BinOp[String, String, String]):
    operator: str = '&'

    def _compute_output(self) -> String:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert isinstance(lhs, String) and isinstance(rhs, String)
        length_min = lhs.length.value_min + rhs.length.value_min
        length_max = lhs.length.value_max + rhs.length.value_max
        length = Integer(
            value_min=min(length_min, Integer.INT_MAX),
            value_max=min(length_max, Integer.INT_MAX),
        )
        content = lhs.content.copy()
        lhs_length = lhs.length.concrete()
        if lhs_length is not None:
            for i, value in rhs.content.items():
                content[lhs_length + i] = value
        return String(length=length, content=content)

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(String(), String())]


@dataclasses.dataclass
class BinOpAnd(BinOp[_BoolIntT, _BoolIntT, _BoolIntT]):
    operator: str = 'And'

    def _compute_output(self) -> _BoolIntT:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert (isinstance(lhs, Boolean) and isinstance(rhs, Boolean)) or (
            isinstance(lhs, Integer) and isinstance(rhs, Integer))
        return lhs & rhs

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Boolean(), Boolean()), (Integer(), Integer())]


@dataclasses.dataclass
class BinOpOr(BinOp[_BoolIntT, _BoolIntT, _BoolIntT]):
    operator: str = 'Or'

    def _compute_output(self) -> _BoolIntT:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert (isinstance(lhs, Boolean) and isinstance(rhs, Boolean)) or (
            isinstance(lhs, Integer) and isinstance(rhs, Integer))
        return lhs | rhs

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Boolean(), Boolean()), (Integer(), Integer())]


_IntStrT = typing.TypeVar('_IntStrT', Integer, String)


@dataclasses.dataclass
class BinOpLt(BinOp[Boolean, _IntStrT, _IntStrT]):
    operator: str = '<'

    def _compute_output(self) -> Boolean:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert (isinstance(lhs, Integer) and isinstance(rhs, Integer)) or (
            isinstance(lhs, String) and isinstance(rhs, String))
        return lhs < rhs

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Integer(), Integer()), (String(), String())]


@dataclasses.dataclass
class BinOpLe(BinOp[Boolean, _IntStrT, _IntStrT]):
    operator: str = '<='

    def _compute_output(self) -> Boolean:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert (isinstance(lhs, Integer) and isinstance(rhs, Integer)) or (
            isinstance(lhs, String) and isinstance(rhs, String))
        return lhs <= rhs

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Integer(), Integer()), (String(), String())]


@dataclasses.dataclass
class BinOpGt(BinOp[Boolean, _IntStrT, _IntStrT]):
    operator: str = '>'

    def _compute_output(self) -> Boolean:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert (isinstance(lhs, Integer) and isinstance(rhs, Integer)) or (
            isinstance(lhs, String) and isinstance(rhs, String))
        return lhs > rhs

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Integer(), Integer()), (String(), String())]


@dataclasses.dataclass
class BinOpGe(BinOp[Boolean, _IntStrT, _IntStrT]):
    operator: str = '>='

    def _compute_output(self) -> Boolean:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert (isinstance(lhs, Integer) and isinstance(rhs, Integer)) or (
            isinstance(lhs, String) and isinstance(rhs, String))
        return lhs >= rhs

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Integer(), Integer()), (String(), String())]


@dataclasses.dataclass
class BinOpEq(BinOp[Boolean, _IntStrT, _IntStrT]):
    operator: str = '='

    def _compute_output(self) -> Boolean:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert (isinstance(lhs, Integer) and isinstance(rhs, Integer)) or (
            isinstance(lhs, String) and isinstance(rhs, String))
        return lhs == rhs

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Integer(), Integer()), (String(), String())]


@dataclasses.dataclass
class BinOpNe(BinOp[Boolean, _IntStrT, _IntStrT]):
    operator: str = '<>'

    def _compute_output(self) -> Boolean:
        lhs, rhs = self.inputs[0].value, self.inputs[1].value
        assert (isinstance(lhs, Integer) and isinstance(rhs, Integer)) or (
            isinstance(lhs, String) and isinstance(rhs, String))
        return lhs != rhs

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Integer(), Integer()), (String(), String())]


@dataclasses.dataclass
class BuiltinCall(OpNode[_ValueT], typing.Generic[_ValueT]):
    name: str

    def __str__(self) -> str:
        return f'{self.name}({", ".join(map(str, self.inputs))})'


@dataclasses.dataclass
class BuiltinCallAsc(BuiltinCall[Integer]):
    inputs: tuple[ValueNode[String]]
    name: str = 'Asc'

    def _compute_output(self) -> Integer:
        arg = self.inputs[0].value
        assert isinstance(arg, String)
        assert arg.length.value_min >= 1
        return arg.content[0]

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(String(length=Integer(value_min=1)),)]


@dataclasses.dataclass
class BuiltinCallChr(BuiltinCall[String]):
    inputs: tuple[ValueNode[Integer]]
    name: str = 'Chr'

    def _compute_output(self) -> String:
        arg = self.inputs[0].value
        assert isinstance(arg, Integer)
        assert arg.value_min >= 0x00 and arg.value_max <= 0xff
        result = String(length=Integer.abstract(1))
        result.content[0] = arg
        return result

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Integer(value_min=0x00, value_max=0xff),)]


@dataclasses.dataclass
class BuiltinCallCBool(BuiltinCall[Boolean]):
    inputs: tuple[ValueNode[typing.Union[Boolean, Integer, IntegerArray, String]]]
    name: str = 'CBool'

    def _compute_output(self) -> Boolean:
        arg = self.inputs[0].value
        if isinstance(arg, Boolean):
            return arg
        if isinstance(arg, String):
            arg = arg.length
        if isinstance(arg, Integer):
            zero = Integer.abstract(0)
            if not arg.contains(zero):
                return Boolean.abstract(True)
            if zero.contains(arg):
                return Boolean.abstract(False)
            return Boolean()
        assert isinstance(arg, IntegerArray)
        return Boolean.abstract(True)

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Boolean(),), (Integer(),), (IntegerArray(),), (String(),)]


@dataclasses.dataclass
class BuiltinCallCInt(BuiltinCall[Integer]):
    inputs: tuple[ValueNode[typing.Union[Boolean, Integer, String]]]
    name: str = 'CInt'

    def _compute_output(self) -> Integer:
        arg = self.inputs[0].value
        if isinstance(arg, Boolean):
            value_bool = arg.concrete()
            if value_bool is None:
                return Integer.abstract(0, 1)
            return Integer.abstract(1 if value_bool else 0)
        if isinstance(arg, Integer):
            return arg
        assert isinstance(arg, String)
        return Integer()

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [
            (Boolean(),),
            (Integer(),),
            # Worst case: &H + 16 hex digits
            (String(length=Integer(value_min=0, value_max=18)),)
        ]


@dataclasses.dataclass
class BuiltinCallCStr(BuiltinCall[String]):
    inputs: tuple[ValueNode[typing.Union[Boolean, Integer, IntegerArray, String]]]
    name: str = 'CStr'

    def _compute_output(self) -> String:
        arg = self.inputs[0].value
        if isinstance(arg, Boolean):
            value_bool = arg.concrete()
            if value_bool is not None:
                return String.abstract(b'True' if value_bool else b'False')
            return String.abstract(b'True', b'False')
        if isinstance(arg, Integer):
            value_int = arg.concrete()
            if value_int is not None:
                return String.abstract(str(value_int).encode())
            return String()
        if isinstance(arg, IntegerArray):
            value_arr = arg.concrete()
            if value_arr is not None:
                return String.abstract(f'[{", ".join(map(str, value_arr))}]'.encode())
            return String()
        assert isinstance(arg, String)
        return arg

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Boolean(),), (Integer(),), (IntegerArray(),), (String(),)]


@dataclasses.dataclass
class BuiltinCallDescribe(BuiltinCall[String]):
    inputs: tuple[ValueNode[typing.Union[Boolean, Integer, IntegerArray, String]]]
    name: str = 'Describe'

    def _compute_output(self) -> String:
        arg = self.inputs[0].value
        if isinstance(arg, Boolean):
            return String.abstract(b'[Boolean]')
        if isinstance(arg, Integer):
            return String.abstract(b'[Integer]')
        if isinstance(arg, IntegerArray):
            length = arg.length.concrete()
            if length is not None:
                return String.abstract(f'[Integer({length})]'.encode())
            prefix = b'[Integer('
            suffix = b')]'
            s_length_min_len = len(str(arg.length.value_min))
            s_length_max_len = len(str(arg.length.value_max))
            s_mid_min_len = len(prefix) + s_length_min_len
            s_mid_max_len = len(prefix) + s_length_max_len
            s_min_len = s_mid_min_len + len(suffix)
            s_max_len = s_mid_max_len + len(suffix)
            s = String(length=Integer(value_min=s_min_len, value_max=s_max_len))
            for i, b in enumerate(prefix):
                s.content[i] = Integer.abstract(b)
            for i in range(s_length_min_len):
                s.content[len(prefix) + i] = Integer(value_min=ord('0'), value_max=ord('9'))
            for i in range(s_length_max_len - s_length_min_len):
                s.content[s_mid_min_len + i] = Integer(value_min=ord(')'), value_max=ord(']'))
            for i, b in enumerate(suffix):
                s.content[s_mid_max_len + i] = Integer.abstract(b)
            return s
        assert isinstance(arg, String)
        return String.abstract(b'[String]')

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(Boolean(),), (Integer(),), (IntegerArray(),), (String(),)]


@dataclasses.dataclass
class BuiltinCallLen(BuiltinCall[Integer]):
    inputs: tuple[ValueNode[String]]
    name: str = 'Len'

    def _compute_output(self) -> Integer:
        arg = self.inputs[0].value
        assert isinstance(arg, String)
        return arg.length

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [(String(),)]


@dataclasses.dataclass
class BuiltinCallMid(BuiltinCall[String]):
    inputs: typing.Union[
        tuple[ValueNode[String], ValueNode[Integer]],
        tuple[ValueNode[String], ValueNode[Integer], ValueNode[Integer]],
    ]
    name: str = 'Mid'

    def _compute_output(self) -> String:
        s, start = self.inputs[0].value, self.inputs[1].value
        assert isinstance(s, String)
        assert isinstance(start, Integer) and start.value_min >= 1
        start_idx_min = start.value_min - 1
        start_idx_max = start.value_max - 1
        if len(self.inputs) == 3:
            length = typing.cast(tuple[ValueNode[String], ValueNode[Integer], ValueNode[Integer]],
                self.inputs)[2].value
            assert isinstance(length, Integer) and length.value_min >= 0
        else:
            length = Integer.abstract(Integer.INT_MAX)
        result = String(length=Integer(
            value_min=min(length.value_min, max(0, s.length.value_min - start_idx_max)),
            value_max=min(length.value_max, max(0, s.length.value_max - start_idx_min)),
        ))
        start_int = start.concrete()
        if start_int is not None:
            start_idx = start_int - 1
            end_idx_max = start_idx + result.length.value_max
            for (idx, char) in s.content.items():
                if start_idx <= idx < end_idx_max:
                    result.content[idx - start_idx] = char
        return result

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        return [
            (String(), Integer(value_min=1)),
            (String(), Integer(value_min=1), Integer(value_min=0)),
        ]


@dataclasses.dataclass
class BuiltinCallString(BuiltinCall[String]):
    inputs: tuple[ValueNode[Integer], ValueNode[typing.Union[Integer, String]]]
    name: str = 'String'

    def _compute_output(self) -> String:
        length, char = self.inputs[0].value, self.inputs[1].value
        assert isinstance(length, Integer) and length.value_min >= 0
        if isinstance(char, String):
            assert char.length.value_min >= 1
            char_int = char.content[0]
        else:
            assert isinstance(char, Integer)
            assert char.value_min >= 0x00 and char.value_max <= 0xff
            char_int = char
        return String(length=length, content=collections.defaultdict(lambda: char_int))

    @staticmethod
    def get_input_bounds() -> list[tuple[Value, ...]]:
        # Limit allocation size.
        length = Integer(value_min=0, value_max=100)
        return [
            (length, Integer(value_min=0x00, value_max=0xff)),
            (length, String(length=Integer(value_min=1))),
        ]


class ValueNodePool:
    def __init__(self):
        self._nodes = []
        self._sticky = []

    def add(self, node: ValueNode, sticky: bool = False) -> None:
        self._nodes.append(node)
        self._sticky.append(sticky)

    def rand_bounded(self, bounds: list[tuple[Value, ...]],
                     no_literal: bool = False) -> typing.Optional[tuple[ValueNode, ...]]:
        for bound in random.sample(bounds, len(bounds)):
            result = []
            used_nonsticky = set()
            for param in bound:
                for idx in random.sample(range(len(self._nodes)), len(self._nodes)):
                    if idx in used_nonsticky:
                        continue
                    node = self._nodes[idx]
                    if no_literal and isinstance(node, Literal):
                        continue
                    if param.contains(node.value):
                        result.append(node)
                        if not self._sticky[idx]:
                            used_nonsticky.add(idx)
                        break
                else:
                    break
            if len(result) == len(bound):
                for idx in sorted(used_nonsticky, reverse=True):
                    self._remove(idx)
                return tuple(result)
        return None

    def _remove(self, idx: int) -> None:
        del self._nodes[idx]
        del self._sticky[idx]


class IdentGenerator:
    def __init__(self):
        self._known = set([
            'And',
            'As',
            'Boolean',
            'ByRef',
            'ByVal',
            'Dim',
            'End',
            'False',
            'Function',
            'If',
            'Integer',
            'Not',
            'Or',
            'String',
            'Then',
            'True',
            'While',
        ])
        for op_type in OpNode.SUBCLASSES:
            if issubclass(op_type, BuiltinCall):
                self._known.add(op_type.name)
        self._known.add('RandSeed')
        self._known.add('CryptoRand')
        self._known.add('FastRand')

    def add_known(self, ident: str) -> None:
        self._known.add(ident)

    def generate(self) -> str:
        while True:
            length = random.randint(4, 8)
            charset = string.ascii_letters + '_'
            ident = random.choice(charset)
            charset += string.digits
            ident += ''.join(random.choices(charset, k=length-1))
            if ident not in self._known:
                self.add_known(ident)
                return ident


_ScopeT = typing.TypeVar('_ScopeT', bound=Scope)


class GeneratorScope(typing.Generic[_ScopeT]):
    def __init__(self, scope: _ScopeT, ident_gen: IdentGenerator,
                 parent: typing.Optional['GeneratorScope'] = None,
                 dead: bool = False):
        self._scope = scope
        self._ident_gen = ident_gen
        # If this is a dead scope, deep-copy the parent scope so that
        # side-effects won't propagate but values are still readable.
        self._parent = copy.deepcopy(parent) if dead else parent
        self._avail_arr_elms: list[tuple[str, int]] = []
        self._value_node_pool = ValueNodePool()
        self._all_value_node_pools = [self._value_node_pool]
        if parent:
            self._all_value_node_pools += parent._all_value_node_pools

    def __str__(self) -> str:
        return str(self._scope)

    @property
    def scope(self) -> _ScopeT:
        return self._scope

    @property
    def vars(self) -> dict[str, Value]:
        return self._scope.vars

    def has_var(self, var: str) -> bool:
        return var in self._scope.vars or (
            self._parent is not None and self._parent.has_var(var))

    def get_var(self, var: str) -> Value:
        try:
            return self._scope.vars[var]
        except KeyError as e:
            if self._parent is not None:
                return self._parent.get_var(var)
            raise e from None

    def new_var(self, value: Value, no_array: bool = False) -> VarLoc:
        # Maybe create the variabile in the parent scope.
        # If this scope is dead, this may lead to usage of undeclared variables
        # because we are working on a dummy deep-copy of the parent scope. This
        # is fine as this scope's code will never be executed.
        if self._parent is not None and random.random() <= 0.5:
            return self._parent.new_var(value, no_array=no_array)

        var = None

        # Integers can be stored as integer array elements.
        if not no_array and isinstance(value, Integer):
            # Maybe use an available integer array element.
            if self._avail_arr_elms and random.random() <= 0.5:
                avail_idx = random.randrange(len(self._avail_arr_elms))
                arr_var, idx = self._avail_arr_elms.pop(avail_idx)
                typing.cast(IntegerArray, self._scope.vars[arr_var]).elements[idx] = value
                var = VarLoc(name=arr_var, idx=Literal(Integer.abstract(idx)))
            # Maybe create a new integer array.
            elif random.random() <= 0.5:
                arr_var = self._ident_gen.generate()
                length = random.randint(1, 10)
                idx = random.randrange(length)
                arr = IntegerArray(length=Integer.abstract(length))
                arr.elements[idx] = value
                self._scope.vars[arr_var] = arr
                for i in range(length):
                    if i != idx:
                        self._avail_arr_elms.append((arr_var, i))
                # Add the array itself to the pool. Because we mutate elements when
                # we use an array element as a variable, the VarUse value will always
                # be correctly updated. Once this VarUse is picked as an input, we
                # must remove the array from self._avail_arr_elms, because a subsequent
                # element modification would not update the operation's output value.
                # This is handled in append_random().
                self._value_node_pool.add(VarUse(
                    value=arr, var=VarLoc(name=arr_var)), sticky=True)
                var = VarLoc(name=arr_var, idx=Literal(Integer.abstract(idx)))

        if var is None:
            # Create a new variable.
            var = VarLoc(name=self._ident_gen.generate())
            self._scope.vars[var.name] = value

        # Add new variable to the pool.
        self._value_node_pool.add(VarUse(value=value, var=var), sticky=True)

        return var

    def append_stmt(self, stmt: Node):
        self._scope.stmts.append(stmt)

    def append_random(self, max_stmts: int) -> int:
        num_stmts = random.randint(1, max_stmts)
        rem_stmts = num_stmts
        while rem_stmts > 0:
            choice = random.randint(0, 3)
            if choice == 0:
                # Add random literal to the pool.
                self.pool_add_rand_literal()
            elif choice == 1:
                # Add random operation to the pool.
                op = OpNode.rand_subtype()
                inputs = self.rand_value_nodes_bounded(op.get_input_bounds())
                if inputs is not None:
                    op_node = op(inputs=inputs)
                    self._value_node_pool.add(op_node)
                    # See the rationale in new_var().
                    for input_node in inputs:
                        if isinstance(input_node, VarUse) and isinstance(input_node.value, IntegerArray):
                            self._invalidate_avail_arr_elms(input_node.var.name)
            elif choice == 2:
                # Append random assignment to new variable (SSA) to program.
                assign_inputs = self.rand_value_nodes_bounded([
                    (Boolean(),), (Integer(),), (String(),),])
                if assign_inputs is not None:
                    rhs, = assign_inputs
                    var = self.new_var(rhs.value)
                    self.append_stmt(AssignStmt(inputs=(rhs,), var=var))
                    rem_stmts -= 1
            elif choice == 3 and rem_stmts > 1:
                # Generate conditional block.
                # Avoid generating literal If True/False Then blocks.
                if_inputs = self.rand_value_nodes_bounded([
                    (Boolean.abstract(True),),
                    (Boolean.abstract(False),),
                ], no_literal=True)
                if if_inputs is not None:
                    if_inputs = typing.cast(tuple[ValueNode[Boolean]], if_inputs)
                    if_node = IfStmt(inputs=if_inputs)
                    cond = if_inputs[0].value.concrete()
                    assert cond is not None
                    # If the condition is false, generated code will be dead.
                    if_scope = GeneratorScope(if_node, self._ident_gen,
                                                parent=self, dead=not cond)
                    rem_stmts -= if_scope.append_random(rem_stmts - 1) + 1
                    self.append_stmt(if_node)
        return num_stmts

    def rand_value_nodes_bounded(self, bounds: list[tuple[Value, ...]],
                                 no_literal: bool = False) -> typing.Optional[tuple[ValueNode, ...]]:
        random.shuffle(self._all_value_node_pools)
        for pool in self._all_value_node_pools:
            node = pool.rand_bounded(bounds, no_literal=no_literal)
            if node is not None:
                return node
        return None

    def pool_add_rand_literal(self) -> None:
        value_type = typing.cast(type[Value], random.choice([Boolean, Integer, String]))
        value = value_type.rand_literal()
        self._value_node_pool.add(Literal(value=value))

    def pool_add(self, node: ValueNode, sticky: bool = False) -> None:
        self._value_node_pool.add(node, sticky=sticky)

    def _invalidate_avail_arr_elms(self, arr_var: str) -> None:
        # May be local.
        self._avail_arr_elms = [
            (var, idx) for var, idx in self._avail_arr_elms
            if var != arr_var
        ]
        # May be in parent.
        if self._parent is not None:
            self._parent._invalidate_avail_arr_elms(arr_var)


class Generator:
    def __init__(self, text: typing.Optional[bytes] = None):
        # Create identifier generator.
        self._ident_gen = IdentGenerator()
        self._ident_gen.add_known('Main')
        self._ident_gen.add_known('text')
        # Create global scope.
        self._global = GeneratorScope(scope=Scope(inputs=()), ident_gen=self._ident_gen)
        # Create main function scope.
        text_value = String.abstract(text) if text is not None else String()
        main_func = Function(name='Main', ret=String, params=(
            Parameter(name='text', typ=String),
        ))
        main_scope = GeneratorScope(main_func, self._ident_gen, parent=self._global)
        main_scope.pool_add(VarUse(value=text_value, var=VarLoc(name='text')), sticky=True)
        # Add some random literals to the main scope to bootstrap generation.
        for _ in range(5):
            main_scope.pool_add_rand_literal()
        self._funcs = {'Main': main_scope}

    def __str__(self) -> str:
        glbls = str(self._global)
        funcs = '\n'.join(map(str, self._funcs.values()))
        return (f'{glbls}\n' if glbls else '') + funcs

    @property
    def funcs(self) -> dict[str, GeneratorScope]:
        return self._funcs

    def split_functions(self, max_funcs: int) -> None:
        num_funcs = random.randint(1, max_funcs)
        while len(self._funcs) < num_funcs:
            # Weight functions by number of statements.
            funcs = [func.scope for func in self._funcs.values()]
            weights = [len(func.stmts) for func in funcs]
            self._split_function(random.choices(funcs, weights, k=1)[0])

    def _split_function(self, orig_func: Function) -> None:
        # Choose a subsequence of statements to split out.
        orig_stmts = orig_func.stmts
        assert len(orig_stmts) > 0
        split_start = random.randrange(0, len(orig_stmts))
        split_end = random.randint(split_start + 1, len(orig_stmts))
        split_stmts = orig_stmts[split_start:split_end]

        # Find all read and write accesses to function-scope variables.
        # We do not need to remap globals and variables in nested scopes.
        # Also find reassigned variables (only happends for *Rand, SSA otherwise).
        func_scope_vars = set(orig_func.vars) | set(param.name for param in orig_func.params)
        func_scope_vars.add(orig_func.name)
        reads: typing.Mapping[str, list[tuple[VarLoc, Value]]] = collections.defaultdict(list)
        writes: typing.Mapping[str, list[tuple[VarLoc, Value]]] = collections.defaultdict(list)
        reassigns: typing.Mapping[str, list[tuple[VarLoc, Value]]] = collections.defaultdict(list)
        for stmt in split_stmts:
            for node in stmt.iter_tree():
                if isinstance(node, VarUse):
                    if node.var.name in func_scope_vars:
                        reads[node.var.name].append((node.var, node.value))
                elif isinstance(node, AssignStmt):
                    if node.var.name in func_scope_vars:
                        writes[node.var.name].append((node.var, node.inputs[0].value))
                elif isinstance(node, FunctionCall):
                    if node.name in self._funcs:
                        # Assume side-effects on variables passed by reference.
                        for param, arg in zip(self._funcs[node.name].scope.params, node.inputs):
                            if param.is_ref and isinstance(arg, VarUse):
                                if arg.var.name in func_scope_vars:
                                    writes[arg.var.name].append((arg.var, type(arg.value)()))
                                    reassigns[arg.var.name].append((arg.var, type(arg.value)()))
                    elif node.name == 'CryptoRand' or node.name == 'FastRand':
                        # {Crypto,Fast}Rand have side-effects on the by-ref first parameter.
                        if node.inputs and isinstance(node.inputs[0], VarUse):
                            dst = node.inputs[0]
                            if dst.var.name in func_scope_vars:
                                writes[dst.var.name].append((dst.var, String()))
                                reassigns[dst.var.name].append((dst.var, String()))
        # This could be set(reads) | set(writes), but we want a deterministic
        # iteration order given the same random seed. Python makes no guarantee
        # on set ordering, but it does (from 3.7) on dict ordering.
        all_var_names = [name for name in reads if name not in writes] + list(writes)

        # Extract all array variable names.
        has_arr_access = lambda accesses : any(
            var.idx is not None or isinstance(value, IntegerArray)
            for var, value in accesses)
        array_names = set(
            name for name in all_var_names
            if (name in reads and has_arr_access(reads[name])) or 
                (name in writes and has_arr_access(writes[name]))
        )

        # Pick new function name.
        func_name = self._ident_gen.generate()

        # Pick a written variable as return value.
        ret_var_orig: typing.Optional[str] = None
        ret_var_new: typing.Optional[str] = None
        ret_value_node: typing.Optional[ValueNode] = None
        ret_type: typing.Optional[type[Value]] = None
        if orig_func.name in writes:
            # If the original function returns, pass it
            # through the new function's return value.
            ret_var_orig = orig_func.name
            ret_var_new = func_name
            ret_type = orig_func.ret
        else:
            # Try to pick a random non-reassigned written variable that is not an array.
            write_vars_noelm = [name for name in writes
                if name not in reassigns and name not in array_names]
            if write_vars_noelm:
                # Pick a random non-array written variable.
                ret_var_orig = random.choice(write_vars_noelm)
                ret_type = type(writes[ret_var_orig][0][1])
                if ret_var_orig not in reads:
                    # Never read, we can just pass through.
                    ret_var_new = func_name
            else:
                # No suitable written variables: return a random value.
                ret_type = typing.cast(type[Value], random.choice([Boolean, Integer, String]))
                ret_value_node = Literal(ret_type.rand_literal())

        # Build parameters and mapping between existing variables and parameters.
        params: list[Parameter] = []
        var_map: dict[str, str] = {}
        var_map_inv: dict[str, str] = {}
        for orig_name in all_var_names:
            if orig_name == ret_var_orig:
                continue
            new_name = self._ident_gen.generate()
            is_written = orig_name in writes
            param_type = typing.cast(type[Value],
                type((writes if is_written else reads)[orig_name][0][1])
                if orig_name not in array_names else IntegerArray)
            params.append(Parameter(name=new_name, typ=param_type, is_ref=is_written))
            var_map[orig_name] = new_name
            var_map_inv[new_name] = orig_name
        random.shuffle(params)

        # Create function generator.
        assert issubclass(ret_type, (Boolean, Integer, String))
        func = Function(name=func_name, params=tuple(params), ret=ret_type)
        func_gen = GeneratorScope(func, self._ident_gen, parent=self._global)

        # Map return variable.
        if ret_var_orig is not None:
            if ret_var_new is None:
                # Earlier we picked a written variable which is also read.
                # Use an intermediate local variable and return it at the end.
                ret_value = writes[ret_var_orig][0][1]
                ret_var_new = func_gen.new_var(ret_value, no_array=True).name
                ret_value_node = VarUse(var=VarLoc(name=ret_var_new), value=ret_value)
            var_map[ret_var_orig] = ret_var_new

        # Map existing variables to new ones.
        # A deep copy is required because VarUse instances from pools are reused,
        # so remapping could have side-effects on the original function.
        split_stmts = copy.deepcopy(split_stmts)
        for stmt in split_stmts:
            for node in stmt.iter_tree():
                if isinstance(node, VarUse) or isinstance(node, AssignStmt):
                    try:
                        node.var = VarLoc(name=var_map[node.var.name], idx=node.var.idx)
                    except KeyError:
                        # This happens because VarUse instances from pools are
                        # reused, so node.var could be already mapped.
                        pass

        # Build new function body.
        for stmt in split_stmts:
            func_gen.append_stmt(stmt)
        if ret_value_node is not None:
            func_gen.append_stmt(AssignStmt(
                var=VarLoc(name=func.name), inputs=(ret_value_node,)))
        self._funcs[func.name] = func_gen

        # Replace statements in original function with call to new function.
        # We're not really doing analysis after this, so it's okay to widen to top
        # for return values and parameter values.
        call = FunctionCall(name=func.name, value=ret_type(), inputs=tuple(
            VarUse(var=VarLoc(name=var_map_inv[param.name]), value=param.typ())
            for param in params
        ))
        orig_stmts[split_start:split_end] = [
            AssignStmt(var=VarLoc(name=ret_var_orig), inputs=(call,))
            if ret_var_orig is not None else call
        ]


def _indent_lines(s: str, depth: int = 1, width: int = 4) -> str:
    indent = ' ' * width * depth
    result = ''
    while True:
        pos = s.find('\n')
        if pos == -1:
            break
        result += indent + s[:pos+1]
        s = s[pos+1:]
    if s:
        result += indent + s
    return result


def  _c_int_div(a: int, b: int) -> int:
    # Python's integer division rounds towards negative infinity,
    # while C rounds towards zero. This makes a difference for
    # negative results. We coudl use float division followed by
    # integer truncation to replicate C's behavior, however, our
    # operands are 64-bit integers, so the result may not fit into
    # a double. Perform an integer division on the absolute values
    # of the operand, which will have the same rouding behavior as
    # C, and apply the sign afterwards.
    result = abs(a) // abs(b)
    if (a < 0) != (b < 0):
        result = -result
    return result


def test() -> None:
    seed = random.randint(0, 10000000000000000000)
    print(f'Seed: {seed}')
    random.seed(seed)
    gen = Generator()
    main = gen.funcs['Main']
    main.append_random(20)
    gen.split_functions(5)
    print(gen)
    print('----------')
    for name, value in main.vars.items():
        print(f'{name} = {repr(value)}')


if __name__ == '__main__':
    test()
