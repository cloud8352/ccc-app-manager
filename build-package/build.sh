#!/bin/bash

###############设定编译变量
VERSION="0.0.5-t1"

echo "build.sh修改自柚柚的 https://gitee.com/deepin-opensource/one-cuter"

echo "检查依赖"

DEPEND=`qmake -v`
if [ "$DEPEND" = "" ] ; then 
echo "未安装依赖：qt5-defalut 本脚本退出"
exit 0
fi

DEPEND=`g++ --version`
if [ "$DEPEND" = "" ] ; then 
echo "未安装依赖：g++ 本脚本退出"
exit 0
fi

DEPEND=`which fakeroot`
if [ "$DEPEND" = "" ] ; then 
echo "未安装依赖：fakeroot 本脚本退出"
exit 0
fi

DEPEND=`dpkg -l | grep libdtkwidget-dev`
if [ "$DEPEND" = "" ] ; then 
echo "未安装依赖：libdtkwidget-dev 本脚本退出"
exit 0
fi

DEPEND=`dpkg -l | grep libdtkgui-dev`
if [ "$DEPEND" = "" ] ; then 
echo "未安装依赖：libdtkgui-dev 本脚本退出"
exit 0
fi

DEPEND=`dpkg -l | grep qtbase5-dev`
if [ "$DEPEND" = "" ] ; then 
echo "未安装依赖：qtbase5-dev 本脚本退出"
exit 0
fi

DEPEND=`dpkg -l | grep zlib1g-dev`
if [ "$DEPEND" = "" ] ; then 
echo "未安装依赖：zlib 本脚本退出"
exit 0
fi

DEPEND=`dpkg -l | grep libgsettings-qt-dev`
if [ "$DEPEND" = "" ] ; then 
echo "未安装依赖：libgsettings-qt-dev 本脚本退出"
exit 0
fi



echo "依赖检查通过，开始编译"

ARCH=`dpkg --print-architecture`
cd `dirname $0`

# 编译
echo "-------------------"
echo "开始编译"
mkdir -p build
cd build/
qmake ../..
make -j
cd ..
echo "编译完成"
echo "-------------------"
#echo "更新翻译"
#lrelease ./translations/*.ts



#放置编译好的文件

mv build/ccc-app-manager pkg/opt/apps/com.github.ccc-app-manager/files
#cp translations/*.qm dabao/extract/opt/apps/top.yzzi.onecuter/files/translations/
rm -rf build

#打包
echo "构建软件包"
mkdir -p pkg/DEBIAN
SIZE=`du -s ./pkg/opt`
SIZE=`echo ${SIZE%%.*}`
# 生成control文件
echo 生成control文件
echo "版本号为$VERSION，可以在脚本中修改"
echo "检测到编译机的架构为$ARCH"
echo "检测到安装后的目录大小为$SIZE"
##########################写入control
cat  << EOF >pkg/DEBIAN/control
Package: com.github.ccc-app-manager
Priority: optional
Section: unknown
Version: $VERSION
Architecture: $ARCH
Maintainer: keke <243768648@qq.com>
Installed-Size: $SIZE
Depends: libc6 (>= 2.28), libgcc1 (>= 1:3.4) | libgcc-s1(>=12), libgl1, libqt5core5a (>= 5.11.0~rc1), libqt5gui5 (>= 5.8.0), libqt5network5 (>= 5.0.2), libqt5widgets5 (>= 5.0.2), libdtkcore5 (>= 5.4), libdtkgui5 (>= 5.4), libdtkwidget5 (>= 5.4)
Description: manage your applications.
 应用管理器，可查看应用包信息，可卸载和打开应用，可在线或离线提取安装包。支持deepin、uos系统。
Homepage: https://gitee.com/ct243768648/ccc-app-manager

EOF

#########################写入info
cat  << EOF >pkg/opt/apps/com.github.ccc-app-manager/info
{
    "appid": "com.github.ccc-app-manager",
    "name": "ccc-app-manager",
    "version": "$VERSION",
    "arch": ["amd64"],
    "permissions": {
        "autostart": false,
        "notification": false,
        "trayicon": true,
        "clipboard": false,
        "account": false,
        "bluetooth": false,
        "camera": false,
        "audio_record": false,
        "installed_apps": false
    }
}
EOF

find pkg/ -type f -print0 |xargs -0 md5sum > pkg/DEBIAN/md5sums
cd pkg
fakeroot dpkg -b . ../
cd ..

rm pkg/DEBIAN/md5sums
rm pkg/DEBIAN/control
rm pkg/opt/apps/com.github.ccc-app-manager/info
rm pkg/opt/apps/com.github.ccc-app-manager/files/ccc-app-manager
echo "编译结束，按回车退出"
read

