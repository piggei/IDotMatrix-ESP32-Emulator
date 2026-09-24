from pathlib import Path
src=(Path(__file__).resolve().parents[1]/"src"/"IDotMatrix.ino").read_text()
assert "#define FW_RELEASE \"0.5.2-dev\"" in src
assert "#define FW_BUILD 177" in src
assert "struct GraffitiRasterState" in src
assert "data[4]==0x00 || data[4]==0x02" in src
assert "const uint8_t *payload=data+9;" in src
assert "sendTransferAck(0x00,0x02);" in src
assert "sendTransferAck(0x00,0x01);" in src
assert "switchDisplayMode(DISPLAY_GRAFFITI);" in src
assert "resetGraffitiRaster();" in src
assert "data[2]<0x01 || data[2]>0x03" in src
assert 'else if(type==2 && total==(uint32_t)NUM_LEDS*3UL) bulk.format="RAW RGB";' in src
print("graffiti original-hardware raster protocol checks: PASS")
