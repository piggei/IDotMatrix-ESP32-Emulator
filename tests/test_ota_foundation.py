from pathlib import Path
import csv

ROOT = Path(__file__).resolve().parents[1]
HW = (ROOT / "src" / "IDotMatrixHardwareConfig.h").read_text(encoding="utf-8")
OTA_H = (ROOT / "src" / "IDotMatrixOta.h").read_text(encoding="utf-8")
OTA_CPP = (ROOT / "src" / "IDotMatrixOta.cpp").read_text(encoding="utf-8")
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")
PIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")
PART = ROOT / "partitions" / "idotmatrix_waveshare_s3_32mb_ota.csv"


def test_ota_is_profile_gated_and_physically_triggered():
    assert "IDOTMATRIX_DEFAULT_OTA_ENABLED" in HW
    assert "IDOTMATRIX_OTA_TRIGGER_PIN" in HW
    assert "IDOTMATRIX_OTA_TRIGGER_HOLD_MS" in HW
    assert "IDOTMATRIX_OTA_AVAILABLE" in HW
    assert "IDOTMATRIX_DEFAULT_OTA_ENABLED=1" in PIO
    assert "IDOTMATRIX_DEFAULT_OTA_TRIGGER_PIN=0" in PIO


def test_ota_uses_standard_arduino_update_to_inactive_slot():
    assert "#include <Update.h>" in OTA_CPP
    assert "Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)" in OTA_CPP
    assert "Update.write(upload.buf, upload.currentSize)" in OTA_CPP
    assert "Update.end(true)" in OTA_CPP
    assert "Update.abort()" in OTA_CPP
    assert '"/update", HTTP_POST' in OTA_CPP


def test_ota_service_runs_outside_runtime_mutex():
    loop = INO[INO.index("void loop(){"):]
    ota_pos = loop.index("idotOtaLoop(millis())")
    lock_pos = loop.index("lockRuntimeState()")
    assert ota_pos < lock_pos


def test_ota_partition_layout_fills_32mb_without_overlap():
    rows = []
    with PART.open(newline="", encoding="utf-8") as fh:
        for raw in fh:
            raw = raw.strip()
            if not raw or raw.startswith("#"):
                continue
            row = next(csv.reader([raw], skipinitialspace=True))
            name, ptype, subtype, offset, size = [x.strip() for x in row[:5]]
            rows.append((name, ptype, subtype, int(offset, 0), int(size, 0)))

    assert [r[0] for r in rows] == ["nvs", "otadata", "ota_0", "ota_1", "spiffs", "coredump"]
    ota0 = next(r for r in rows if r[0] == "ota_0")
    ota1 = next(r for r in rows if r[0] == "ota_1")
    fs = next(r for r in rows if r[0] == "spiffs")
    assert ota0[4] == 0x300000
    assert ota1[4] == 0x300000
    assert ota0[3] + ota0[4] == ota1[3]
    assert ota1[3] + ota1[4] == fs[3]
    coredump = next(r for r in rows if r[0] == "coredump")
    assert fs[3] + fs[4] == coredump[3]
    assert coredump[3] + coredump[4] == 0x2000000


def test_ota_header_has_disabled_stubs_for_other_profiles():
    assert "inline void idotOtaBegin" in OTA_H
    assert "inline void idotOtaLoop" in OTA_H
    assert 'return "disabled"' in OTA_H


def test_ota_captive_portal_uses_wildcard_dns_and_probe_redirects():
    assert "#include <DNSServer.h>" in OTA_CPP
    assert 'dnsServer.start(53, "*", apIp)' in OTA_CPP
    assert "dnsServer.processNextRequest()" in OTA_CPP
    assert 'server.sendHeader("Location", portalUrl(), true)' in OTA_CPP
    for path in [
        "/generate_204",
        "/gen_204",
        "/hotspot-detect.html",
        "/library/test/success.html",
        "/connecttest.txt",
        "/ncsi.txt",
        "/fwlink",
    ]:
        assert f'server.on("{path}", HTTP_GET, redirectToPortal)' in OTA_CPP
    assert "server.onNotFound([]()" in OTA_CPP
    assert "redirectToPortal();" in OTA_CPP
    assert 'server.on("/health", HTTP_GET' in OTA_CPP
