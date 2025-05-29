#!/usr/bin/env python3
#
# Copyright 2020-2025 AVSystem <avsystem@avsystem.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import base64

from mbed import mbed
from contextlib import contextmanager
import argparse
import json
import os
import subprocess
import sys

PYTHON = sys.executable or 'python3'
ROOT_DIR = os.path.dirname(os.path.realpath(__file__))
APP_CONFIG_FILE = os.path.join(ROOT_DIR, 'mbed_app.json')


def _prepare_env(target):
    with open('.mbed', 'w') as f:
        f.write('ROOT=.\n')
        f.write('TARGET=%s\n' % (target,))
        f.write('TOOLCHAIN=GCC_ARM\n')
    mbed.deploy()


@contextmanager
def _safe_chdir(path):
    current_dir = os.getcwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(current_dir)


def _get_known_targets():
    with open(os.path.join(ROOT_DIR, 'mbed_app.json'), 'r') as cfg:
        config = json.load(cfg)

    return [target for target in config['target_overrides'].keys() if target != '*']


def _traverse_config(config, *paths):
    def traverse_config_impl(path):
        result = config
        for key in path:
            if result is None:
                return None
            result = result.get(key, None)
        return result

    for path in paths:
        result = traverse_config_impl(path)
        if result is not None:
            return result

    return result


def _compile_bootloader(target):
    bootloader_rootdir = os.path.join(ROOT_DIR, 'mbed-bootloader')
    with _safe_chdir(bootloader_rootdir):
        with open('.mbed', 'w') as f:
            f.write('ROOT=.\n')
        os.chdir('presets')
        subprocess.check_call([PYTHON, 'build_presets.py', target])
        subprocess.check_call([PYTHON, 'export.py', os.path.join(ROOT_DIR, 'mbed-bootloader-bin')])


def _get_or_create_manifest_dev_config():
    manifest_dev_config = os.path.join(ROOT_DIR, '.manifest-dev-tool', 'dev.cfg.yaml')
    if not os.path.isfile(manifest_dev_config):
        with _safe_chdir(ROOT_DIR):
            subprocess.check_call([PYTHON, '-m', 'manifesttool.dev_tool.dev_tool', 'init'])
        # Remove update_default_resources.c, manifest-dev-tool creates it but we don't want it
        try:
            os.unlink(os.path.join(ROOT_DIR, 'update_default_resources.c'))
        except FileNotFoundError:
            pass
    return manifest_dev_config


def _load_cert_as_der(cert_file):
    with open(cert_file, 'rb') as f:
        cert = f.read()

    if b'-----BEGIN' in cert:
        # Probably a PEM file
        import cryptography
        import cryptography.hazmat
        import cryptography.hazmat.primitives
        import cryptography.hazmat.primitives.serialization
        import cryptography.x509
        cert = cryptography.x509.load_pem_x509_certificate(cert).public_bytes(
            cryptography.hazmat.primitives.serialization.Encoding.DER)

    return cert


def _prepare_fota_config(app_config, manifest_config, update_certificate):
    if manifest_config is None:
        manifest_config = _get_or_create_manifest_dev_config()

    import yaml
    with open(manifest_config, 'r') as f:
        manifest_config = yaml.safe_load(f)

    # support both manifest-tool and manifest-dev-tool config formats
    vendor_id = _traverse_config(manifest_config, ['vendor', 'vendor-id'], ['vendor-id'])
    class_id = _traverse_config(manifest_config, ['device', 'class-id'], ['class-id'])
    if update_certificate is None:
        update_certificate = os.path.join(ROOT_DIR, '.manifest-dev-tool', 'dev.cert.der')
    update_certificate = _load_cert_as_der(update_certificate)

    if not vendor_id or not class_id or not update_certificate:
        raise ValueError('vendor-id, class-id and update certificate must be provided for FOTA')

    app_config['target_overrides']['*']['anjay-mbed-fota.update-cert'] = base64.b64encode(
        update_certificate).decode()
    app_config['target_overrides']['*']['anjay-mbed-fota.vendor-id'] = vendor_id
    app_config['target_overrides']['*']['anjay-mbed-fota.class-id'] = class_id
    with open(APP_CONFIG_FILE, 'w') as f:
        json.dump(app_config, f, indent=4)
        f.write('\n')


def _main(args):
    parser = argparse.ArgumentParser(
        description='Initializes the project, by setting up the target and compiling bootloader')
    parser.add_argument('--target', type=str, choices=_get_known_targets(), required=True,
                        help='Name of the board to prepare the build for')
    parser.add_argument('--manifest-config', type=str,
                        help='Configuration file for manifest-tool. If not provided, a development configuration will be used and created if necessary.')
    parser.add_argument('--update-certificate', type=str,
                        help='Certificate file that will be used for verifying the update images. May be empty for development configuration.')
    args = parser.parse_args(args)

    with _safe_chdir(ROOT_DIR):
        _prepare_env(args.target)

        with open(APP_CONFIG_FILE, 'r') as cfg:
            config = json.load(cfg)

        extra_labels = _traverse_config(config, ['target_overrides', args.target,
                                                 'target.extra_labels_add'])
        if any(label.startswith('BL_') for label in (extra_labels or [])):
            _compile_bootloader(args.target)

        fota_enable = _traverse_config(config,
                                       ['target_overrides', args.target, 'anjay-mbed-fota.enable'])
        if fota_enable:
            _prepare_fota_config(config, args.manifest_config, args.update_certificate)


if __name__ == '__main__':
    sys.exit(_main(sys.argv[1:]))
