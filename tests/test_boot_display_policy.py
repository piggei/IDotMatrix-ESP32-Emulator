from pathlib import Path

src = (Path(__file__).resolve().parents[1] / "src" / "IDotMatrix.ino").read_text()
start = src.index("void applyBootDisplayPolicy() {")
end = src.index("#if OLED_STATUS_ENABLED", start)
block = src[start:end]

carousel = block.index("int8_t slot=nextCarouselSlot(-1);")
rtc = block.index("if (rtcReady && rtcTimeValid)")
off = block.index("displayMode=DISPLAY_NONE")

assert carousel < rtc < off, "Boot priority must be stored Carousel -> valid RTC Clock -> screen off"
assert 'BOOT POLICY: stored carousel -> slot ' in block
assert 'BOOT POLICY: no stored carousel + valid RTC -> CLOCK' in block
