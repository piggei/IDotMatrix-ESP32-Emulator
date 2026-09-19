def effective(detected, mount):
    return (detected - mount) % 360

for mount in (0, 90, 180, 270):
    for detected in (0, 90, 180, 270):
        got = effective(detected, mount)
        assert got in (0, 90, 180, 270)

assert effective(0, 0) == 0
assert effective(90, 0) == 90
assert effective(90, 90) == 0
assert effective(180, 90) == 90
assert effective(270, 180) == 90
assert effective(0, 270) == 90
print('orientation mount mapping: PASS')
