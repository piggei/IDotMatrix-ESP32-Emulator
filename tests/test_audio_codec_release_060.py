from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")
HW = (ROOT / "src" / "IDotMatrixHardwareConfig.h").read_text(encoding="utf-8")
PIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")
AUDIO = (ROOT / "src" / "IDotMatrixAudioOutput.cpp").read_text(encoding="utf-8")


def test_release_identity():
    assert '#define FW_RELEASE "0.6.0"' in INO
    assert '#define FW_BUILD 223' in INO


def test_waveshare_audio_profile_is_synth_only():
    assert PIO.count('-DIDOTMATRIX_DEFAULT_AUDIO_CODEC_ENABLED=1') == 3
    assert PIO.count('-DIDOTMATRIX_DEFAULT_CONNECTION_BUZZER_ENABLED=1') == 4
    assert 'IDOTMATRIX_AUDIO_CODEC_AVAILABLE' in HW
    assert 'IDOTMATRIX_AUDIO_CODEC_I2C_ADDRESS 0x18' in HW
    assert 'IDOTMATRIX_AUDIO_I2C_SDA_PIN 47' in HW
    assert 'IDOTMATRIX_AUDIO_I2C_SCL_PIN 48' in HW
    assert 'TwoWire gAudioWire(1)' in AUDIO
    assert 'gAudioWire.begin(IDOTMATRIX_AUDIO_I2C_SDA_PIN, IDOTMATRIX_AUDIO_I2C_SCL_PIN, 400000U)' in AUDIO
    assert 'IDOTMATRIX_AUDIO_I2S_BCLK_PIN 43' in HW
    assert 'IDOTMATRIX_AUDIO_I2S_WS_PIN 38' in HW
    assert 'IDOTMATRIX_AUDIO_I2S_DOUT_PIN 21' in HW
    assert 'IDOTMATRIX_AUDIO_I2S_MCLK_PIN 12' in HW
    assert 'IDOTMATRIX_AUDIO_PA_ENABLE_PIN 11' in HW
    assert 'I2S_NUM_1' in AUDIO
    assert 'kSampleRate = 48000' in AUDIO
    assert 'IDOTMATRIX_BUZZER_FREQUENCY_HZ' in AUDIO
    assert 'BuzzerAudioSamples' not in AUDIO
    assert '.wav' not in AUDIO.lower()
    assert '.pcm' not in AUDIO.lower()


def test_release_enables_all_notification_policies_on_waveshare():
    waveshare = PIO.split('[env:matrixportal_s3_hub75_64]')[0]
    assert waveshare.count('-DIDOTMATRIX_DEFAULT_ALARM_BUZZER_ENABLED=1') == 3
    assert waveshare.count('-DIDOTMATRIX_DEFAULT_COUNTDOWN_BUZZER_ENABLED=1') == 3
    assert waveshare.count('-DIDOTMATRIX_DEFAULT_SCHEDULE_BUZZER_ENABLED=1') == 3
    assert waveshare.count('-DIDOTMATRIX_DEFAULT_CONNECTION_BUZZER_ENABLED=1') == 3


def test_release_uses_only_modern_i2s_api():
    audio = (ROOT / "src" / "IDotMatrixAudioOutput.cpp").read_text()
    assert "#include <driver/i2s_std.h>" in audio
    assert "#include <ESP_I2S.h>" not in audio
    assert "driver/i2s.h" not in audio
    assert "i2s_driver_install" not in audio
    assert "i2s_write(" not in audio
    assert "I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER)" in audio
    assert "i2s_new_channel" in audio
    assert "i2s_channel_init_std_mode" in audio
    assert "i2s_channel_write" in audio


def test_release_preserves_waveshare_reference_codec_gate_and_restores_normal_volume():
    assert 'kBringupCodecVolume' not in AUDIO
    assert 'IDOTMATRIX_AUDIO_CODEC_VOLUME 100' in HW
    assert 'kCodec48k' in AUDIO
    assert '12288000U, 48000U' in AUDIO
    assert 'codecSetSampleRate48k()' in AUDIO
    assert 'codecSetBits16()' in AUDIO
    assert 'codecSetVolume(IDOTMATRIX_AUDIO_CODEC_VOLUME)' in AUDIO
    assert 'digitalWrite(IDOTMATRIX_AUDIO_PA_ENABLE_PIN, HIGH)' in AUDIO
    assert 'AUDIO: ES8311 configured, volume=' in AUDIO
    assert 'AUDIO: PA GPIO' in AUDIO
    assert 'regsToLog' not in AUDIO


def test_release_does_not_change_i2s_driver_family_or_add_samples():
    assert '#include <driver/i2s_std.h>' in AUDIO
    assert 'driver/i2s.h' not in AUDIO
    assert '#include <ESP_I2S.h>' not in AUDIO
    assert 'i2s_channel_write' in AUDIO
    assert '.wav' not in AUDIO.lower()
    assert '.pcm' not in AUDIO.lower()
