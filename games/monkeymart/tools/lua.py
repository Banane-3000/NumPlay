"""Read the data in the web game's Lua sources without running them: table constructors, and the few kinds of
statement its data functions are made of, evaluated by hand (numbers, + - * /, names given in an environment)."""
import re

TOKEN = re.compile(r"""
    (?P<ws>\s+|--\[\[.*?\]\]|--[^\n]*) |
    (?P<num>\d+\.\d*|\.\d+|\d+) |
    (?P<str>"[^"]*"|'[^']*') |
    (?P<name>[A-Za-z_][A-Za-z_0-9]*) |
    (?P<op>==|~=|<=|>=|\.\.|[-+*/%#(){}\[\],;.:=<>])
""", re.S | re.X)


def tokens(src):
    out, i = [], 0
    while i < len(src):
        m = TOKEN.match(src, i)
        if not m:
            raise ValueError('bad Lua at %r' % src[i:i + 30])
        i = m.end()
        if m.lastgroup != 'ws':
            out.append((m.lastgroup, m.group()))
    return out


class Parser:
    """Expressions and table constructors over a token list; names resolve through env (a dict, or a function
    of the dotted name), and unknown ones stay as strings such as 'gameState.updateZavod'."""

    def __init__(self, toks, env=None):
        self.t, self.i, self.env = toks, 0, env or {}

    def peek(self, k=0):
        return self.t[self.i + k] if self.i + k < len(self.t) else (None, None)

    def take(self, want=None):
        tok = self.peek()
        if want is not None and tok[1] != want:
            raise ValueError('expected %r, got %r near %r' % (want, tok[1], self.t[self.i:self.i + 8]))
        self.i += 1
        return tok

    def expr(self):
        v = self.term()
        while self.peek()[1] in ('+', '-'):
            op = self.take()[1]
            w = self.term()
            v = v + w if op == '+' else v - w
        return v

    def term(self):
        v = self.unary()
        while self.peek()[1] in ('*', '/'):
            op = self.take()[1]
            w = self.unary()
            v = v * w if op == '*' else v / w
        return v

    def unary(self):
        if self.peek()[1] == '-':
            self.take()
            return -self.unary()
        if self.peek()[1] == '#':
            self.take()
            return len(self.unary())
        return self.atom()

    def atom(self):
        kind, s = self.peek()
        if kind == 'num':
            self.take()
            return float(s) if '.' in s else int(s)
        if kind == 'str':
            self.take()
            return s[1:-1]
        if s == '{':
            return self.table()
        if s == '(':
            self.take()
            v = self.expr()
            self.take(')')
            return v
        if kind == 'name':
            if s in ('true', 'false'):
                self.take()
                return s == 'true'
            if s == 'nil':
                self.take()
                return None
            name = self.dotted()
            if self.peek()[1] == '(':
                args = self.args()
                if name == 'vmath.vector3':
                    return {'x': args[0], 'y': args[1]}
                return ('call', name, args)
            # a path such as this.listPlaceBoxes[5].x goes to the environment whole
            while self.peek()[1] == '[' and self.peek(1)[0] in ('num', 'name') and self.peek(2)[1] == ']':
                name += '[' + self.peek(1)[1] + ']'
                self.i += 3
                while self.peek()[1] == '.' and self.peek(1)[0] == 'name':
                    self.take()
                    name += '.' + self.take()[1]
            return self.lookup(name)
        raise ValueError('unexpected %r near %r' % (s, self.t[self.i:self.i + 8]))

    def dotted(self):
        name = self.take()[1]
        while self.peek()[1] in ('.', ':') and self.peek(1)[0] == 'name':
            sep = self.take()[1]
            name += sep + self.take()[1]
        return name

    def lookup(self, name):
        if callable(self.env):
            return self.env(name)
        if name in self.env:
            return self.env[name]
        head, _, rest = name.partition('.')
        if head in self.env and rest:
            v = self.env[head]
            for part in rest.split('.'):
                v = v[part]
            return v
        return name

    def args(self):
        self.take('(')
        out = []
        while self.peek()[1] != ')':
            out.append(self.expr())
            if self.peek()[1] == ',':
                self.take()
        self.take(')')
        return out

    def table(self):
        self.take('{')
        arr, rec = [], {}
        while self.peek()[1] != '}':
            if self.peek()[0] == 'name' and self.peek(1)[1] == '=':
                k = self.take()[1]
                self.take('=')
                rec[k] = self.expr()
            elif self.peek()[1] == '[':
                self.take()
                k = self.expr()
                self.take(']')
                self.take('=')
                rec[k] = self.expr()
            else:
                arr.append(self.expr())
            if self.peek()[1] in (',', ';'):
                self.take()
        self.take('}')
        if rec and arr:
            rec['_list'] = arr
            return rec
        return rec if rec else arr


def value_after(src, marker, env=None):
    """the expression after the first `marker =` in src"""
    m = re.search(re.escape(marker) + r'\s*=', src)
    if not m:
        raise KeyError(marker)
    return Parser(tokens(src[m.end():]), env).expr()


def function_body(src, name):
    """the text of `function this:name(...)` (or GameState's `function this:name`), up to its matching end"""
    m = re.search(r'function\s+this[:.]' + re.escape(name) + r'\s*\(', src)
    if not m:
        raise KeyError(name)
    toks = tokens(src[m.end():])
    depth, i = 1, 0
    start = None
    # count block openers: function/if/for/while/do ... end (repeat is not used in these files)
    pos = m.end()
    for mm in re.finditer(r'--[^\n]*|"[^"]*"|\'[^\']*\'|\b(function|if|for|while|end)\b', src[pos:]):
        w = mm.group(1)
        if not w:
            continue
        if w in ('function', 'if'):
            depth += 1
        elif w in ('for', 'while'):
            depth += 1
        elif w == 'end':
            depth -= 1
            if depth == 0:
                return src[pos:pos + mm.start()]
    raise ValueError('no end for ' + name)
