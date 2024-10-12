#!/bin/bash
# compatible with Command Line Tools v5
# https://developer.huawei.com/consumer/cn/doc/harmonyos-guides-V5/ide-command-line-building-app-V5
set -x
set -e
PWD=$(dirname "${BASH_SOURCE[0]}")
cd $PWD/..

echo "Checking Java..."
java -version
echo "Done"

echo "Checking hvigor..."
pushd harmony
hvigorw -v
popd
echo "Done"

echo "Checking npm configuration..."
npm config set registry=https://repo.huaweicloud.com/repository/npm/
npm config set @ohos:registry=https://repo.harmonyos.com/npm/
echo "Done"

echo "Checking Ohpm configration..."
ohpm --version
ohpm config set registry https://ohpm.openharmony.cn/ohpm/
ohpm config set strict_ssl true
echo "Done"

echo "Checking Ohpm dependencies..."
pushd harmony
ohpm install --all
popd
pushd harmony/entry
ohpm install --all
popd
echo "Done"

echo "All tasks - Done"
