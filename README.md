# 应用管理器
应用管理器，可查看应用包信息，可卸载和打开应用，可在线或离线提取安装包。支持deepin、uos系统。
## 构建依赖
debhelper (>= 11),libdtkwidget-dev,libdtkgui-dev,qtbase5-dev,zlib,libgsettings-qt-dev

## 构建安装
```
mkdir build
cd build
qmake ..
make install
```

## 制作软件包
进入`build-package`目录，运行build.sh

构建软件包需要额外安装 `fakeroot`