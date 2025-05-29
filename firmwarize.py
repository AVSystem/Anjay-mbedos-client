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
import contextlib
import itertools
import logging
import os
import subprocess
import sys
import tempfile


def parse_mbed_file():
    values = {}
    try:
        with open('.mbed', 'r') as f:
            for line in f:
                key, val = line.strip().split('=', maxsplit=1)
                values[key] = val
    except ValueError as e:
        logging.warning('Unable to read .mbed file: %s', e)
        raise

    return values


def get_target_name_from_input(input_file):
    abs_path = os.path.abspath(input_file)
    segments_under_build = list(
        itertools.dropwhile(lambda x: x != 'BUILD', abs_path.split(os.path.sep)))[1:]
    if segments_under_build:
        logging.debug('Target detected from input file path: %s', segments_under_build[0])
        return segments_under_build[0]

    logging.debug('Unable to detect target name from input path: %s', input_file)
    return None


def get_target_name_from_mbed_file():
    try:
        target = parse_mbed_file()['TARGET']
        logging.debug('Target detected from .mbed file: %s', target)
        return target
    except ValueError as e:
        logging.debug('Unable to detect target name from .mbed file: %s', e)

    return None


def detect_target_name(input_file):
    if input_file:
        target = get_target_name_from_input(input_file)
        if target:
            return target

    target = get_target_name_from_mbed_file()
    if target:
        return target

    logging.debug('Unable to detect target name from .mbed file: %s', input_file)
    return None


def get_default_input_name(target):
    toolchain = None
    project_name = os.path.basename(os.getcwd())

    try:
        mbed = parse_mbed_file()
        target = target or mbed['TARGET']
        toolchain = mbed['TOOLCHAIN']
    except ValueError:
        pass

    if not target or not toolchain or not project_name:
        logging.debug('Unable to detect input file name')
        return None

    return 'BUILD/{target}/{toolchain}-RELEASE/{project_name}_update.bin'.format(target=target,
                                                                                 toolchain=toolchain,
                                                                                 project_name=project_name)


def get_default_output_name(input_file, target):
    input_dir = os.path.dirname(os.path.abspath(input_file))
    input_no_extension, _ = os.path.splitext(input_file)

    try:
        version = subprocess.check_output(['git', 'describe', '--tags', '--always'], cwd=input_dir,
                                          universal_newlines=True).strip()
    except subprocess.CalledProcessError as e:
        logging.warning('Unable to get version from git: %s', e)
        version = 'SNAPSHOT'

    return '{input_file}_{target}_{version}.pkg'.format(input_file=input_no_extension,
                                                        target=target, version=version)


def pack_firmware(input_file, output_file, manifest_config, manifest_tool_args):
    PYTHON = sys.executable or 'python3'
    DUMMY_URL = ' '
    with contextlib.ExitStack() as stack:
        manifest_file = stack.enter_context(tempfile.NamedTemporaryFile())
        if manifest_config is None:
            logging.debug('Using manifest-dev-tool')
            command = [PYTHON, '-m', 'manifesttool.dev_tool.dev_tool', 'create', '-u', DUMMY_URL,
                       '-p', input_file, '--sign-image']
        else:
            logging.debug('Using manifest-tool')
            import yaml
            manifest_config = yaml.safe_load(manifest_config)
            manifest_config['payload']['file-path'] = input_file
            manifest_config['payload']['format'] = 'raw-binary'
            modified_manifest_config = stack.enter_context(tempfile.NamedTemporaryFile())
            yaml.safe_dump(manifest_config, modified_manifest_config)
            modified_manifest_config.flush()
            command = [PYTHON, '-m', 'manifesttool.mtool.mtool', 'create', '-c',
                       modified_manifest_config.name]
        command += ['-o', manifest_file.name]
        command += manifest_tool_args
        logging.debug('Running: %r', command)
        subprocess.check_call(command)

        with open(output_file, 'wb') as f:
            with open(manifest_file.name, 'rb') as manifest:
                f.write(manifest.read())
            with open(input_file, 'rb') as image:
                f.write(image.read())

    logging.info('DONE! Grab your %s', output_file)


def _main(args):
    LOG_LEVEL = os.getenv('LOGLEVEL', 'info').upper()
    try:
        import coloredlogs
        coloredlogs.install(level=LOG_LEVEL)
    except ImportError:
        logging.basicConfig(level=LOG_LEVEL)

    import argparse

    parser = argparse.ArgumentParser(description=''
                                                 'Converts .bin file to a file applicable during LwM2M Firmware Update\n'
                                                 '\n'
                                                 'environment variables:\n'
                                                 '    LOGLEVEL - adjust log level (debug, info, warning, error)\n'
                                                 '\n',
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('-i', '--input',
                        help='File which should be converted to a LwM2M Firmware Update image')
    parser.add_argument('-o', '--output', help='File to which the result shall be stored')
    parser.add_argument('-m', '--target', help='Target for which the firmware was built')
    parser.add_argument('-f', '--force-overwrite',
                        help='Overwrite existing output file if one exists', action='store_true')
    parser.add_argument('--manifest-config', type=str,
                        help='Configuration file for manifest-tool. If not provided, a development configuration will be used.')
    parser.add_argument('manifest_tool_args', type=str, nargs='*',
                        help='Additional arguments passed to manifest-tool or manifest-dev-tool')
    args = parser.parse_args(args)

    if not args.input:
        args.input = get_default_input_name(args.target)
        if not args.input:
            logging.error('Unable to detect input file name, use --input')
            return 1

        logging.info('Detected input file path: %s (use --input to override)', args.input)

    if not os.path.exists(args.input):
        logging.error('Firmware image %s does not exists, use --input', args.input)
        return 1

    if not args.target:
        args.target = detect_target_name(args.input)
        if not args.target:
            logging.error('Unable to detect target name, use --target')
            return 1

        logging.info('Detected target name: %s (use --target to override)', args.target)

    if not args.output:
        args.output = get_default_output_name(args.input, args.target)
        logging.info('Using output name: %s (use --output to override)', args.output)

    if os.path.exists(args.output) and not args.force_overwrite:
        logging.error('Output file %s already exists (use --force-overwrite to overwrite)',
                      args.output)
        return 1

    if not args.input.endswith('_update.bin'):
        logging.warning(
            'Input filename (%s) does not end with _update.bin, did you pass the correct input file?',
            args.input)

    pack_firmware(args.input, args.output, args.manifest_config, args.manifest_tool_args)
    return 0


if __name__ == '__main__':
    sys.exit(_main(sys.argv[1:]))
