from pathlib import Path

src=(Path(__file__).resolve().parents[1]/"src"/"IDotMatrix.ino").read_text().splitlines()
stack=[]
for lineno,line in enumerate(src,1):
    stripped=line.lstrip()
    if stripped.startswith(("#if ","#ifdef ","#ifndef ")):
        stack.append((lineno,stripped))
    elif stripped.startswith("#endif"):
        assert stack, f"unmatched #endif at line {lineno}"
        stack.pop()
assert not stack, "unterminated conditional(s): " + ", ".join(f"line {n}: {d}" for n,d in stack)
print("preprocessor directive balance: PASS")
