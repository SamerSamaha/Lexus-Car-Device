#!/bin/sh
# Creates the test-only Python virtual environment and installs tools/requirements/test.txt
# into it. Nothing is installed system-wide.
#   tools/setup_test_venv.sh [venv-folder]     default: $HOME/.venvs/lexus-car-device
# CMake finds the interpreter as LEXUS_HEAD_UNIT_TEST_PYTHON (default: that folder's
# bin/python3). Debian's own python3 has no ensurepip, so when "python3 -m venv" cannot add pip
# the environment is made without pip and pip is bootstrapped inside it from PyPA's get-pip.py.
set -eu
repository_root=$(cd "$(dirname "$0")/.." && pwd)
venv_folder=${1:-"$HOME/.venvs/lexus-car-device"}
if [ ! -x "$venv_folder/bin/python3" ]; then
    if ! python3 -m venv "$venv_folder" > /dev/null 2>&1; then
        rm -rf "$venv_folder"
        python3 -m venv --without-pip "$venv_folder"
        download_folder=$(mktemp -d)
        python3 -I -c "import urllib.request; urllib.request.urlretrieve('https://bootstrap.pypa.io/get-pip.py', '$download_folder/get-pip.py')"
        "$venv_folder/bin/python3" -I "$download_folder/get-pip.py" --quiet
        rm -rf "$download_folder"
    fi
fi
"$venv_folder/bin/python3" -m pip install --quiet --requirement "$repository_root/tools/requirements/test.txt"
"$venv_folder/bin/python3" -c "import cantools; print('cantools', cantools.__version__, 'in', '$venv_folder')"
