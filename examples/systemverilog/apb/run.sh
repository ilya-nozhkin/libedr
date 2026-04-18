#!/bin/bash

SCRIPT_DIR=$(dirname $0)

if [[ ! -v EDR_INSTALL_DIR ]]; then
  echo "EDR_INSTALL_DIR environment variable is not set. Point it to a libedr installation directory."
  exit
fi

export LD_LIBRARY_PATH=${EDR_INSTALL_DIR}/lib

${SCRIPT_DIR}/obj_dir/Vtestbench
