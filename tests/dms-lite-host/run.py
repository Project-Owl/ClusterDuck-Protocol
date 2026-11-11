#!/usr/bin/env python3
"""Compile both actual sketches and check their output, without devices/network."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dms-lite-repo', type=Path, help='optional DMS Lite Docker checkout')
    parser.add_argument('--dms-lite-ref', default='origin/modern-update')
    args = parser.parse_args()
    deps = Path(os.environ.get('CDP_LIBDEPS', ROOT / '.pio/libdeps/local_heltec_wifi_lora_32_V3'))
    arduino_json = Path(os.environ.get('ARDUINOJSON_SRC', deps / 'ArduinoJson/src'))
    crc = Path(os.environ.get('CRC32_SRC', deps / 'CRC32/src'))
    if not arduino_json.is_dir() or not crc.is_dir():
        parser.error('Build an example for local_heltec_wifi_lora_32_V3 first, or set CDP_LIBDEPS / ARDUINOJSON_SRC / CRC32_SRC.')

    decoders = []
    if args.dms_lite_repo:
        def git(*arguments):
            return subprocess.check_output(['git', '-C', str(args.dms_lite_repo), *arguments], text=True)
        revision = git('rev-parse', args.dms_lite_ref).strip()
        for folder, filename in [('serial-sql-writer', 'serial_sqlwriter.py'), ('wifi-sql-writer', 'wifi_sqlwriter.py')]:
            source = git('show', f'{revision}:{folder}/{filename}')
            namespace = {'__name__': f'dms_lite_compat_{folder}'}
            exec(compile(source, filename, 'exec'), namespace)
            decoders.append(namespace['decode_packet'])
        print(f'Checking both DMS Lite decoders at {revision}', flush=True)

    with tempfile.TemporaryDirectory(prefix='cdp-dms-lite-') as directory:
        results = []
        for mode in ('serial', 'mqtt'):
            binary = Path(directory) / mode
            command = [os.environ.get('CXX', 'clang++'), '-std=c++20', '-g', '-O1',
                       '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                       '-Wno-deprecated-declarations', '-Wno-c++11-narrowing',
                       '-include', str(HERE.parent / 'routing-host/stubs/common.h'),
                       '-I', str(HERE / 'stubs'), '-I', str(HERE.parent / 'routing-host/stubs'),
                       '-I', str(ROOT / 'src'), '-I', str(arduino_json), '-I', str(crc),
                       str(HERE / 'sketch.cpp'), str(ROOT / 'src/CdpPacket.cpp'),
                       str(ROOT / 'src/utils/DuckUtils.cpp'), str(crc / 'CRC32.cpp'), '-o', str(binary)]
            if mode == 'mqtt':
                command.append('-DDMS_LITE_MQTT')
            subprocess.run(command, check=True)
            raw = subprocess.check_output([str(binary)])
            messages = raw.splitlines()
            for message in messages:
                packet = json.loads(message.decode('utf-8'))
                for key in ('DeviceID', 'MessageID', 'topic', 'Payload', 'path'):
                    assert isinstance(packet[key], str), (mode, key)
                for key in ('DeviceID', 'MessageID', 'topic'):
                    assert packet[key].strip(), (mode, key)
                for key in ('hops', 'duckType'):
                    assert type(packet[key]) is int and packet[key] >= 0, (mode, key)
                for decode in decoders:
                    assert decode(message) == packet
            results.append(messages)
            print(f'PASS: {mode}, {len(messages)} emitted packets accepted' + (' by both DMS Lite adapters' if decoders else ''), flush=True)
        assert results[0] == results[1], 'serial and MQTT emitted different data'
        print('PASS: serial and MQTT wire formats match', flush=True)


if __name__ == '__main__':
    main()
