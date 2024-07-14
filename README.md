# 应用管理器
应用管理器，可查看应用包信息，可卸载和打开应用，可在线或离线提取安装包。支持deepin、uos系统。
## 构建依赖
debhelper (>= 11),libdtkwidget-dev,libdtkgui-dev,qtbase5-dev,zlib1g-dev,libgsettings-qt-dev

## 构建安装
```
mkdir build
cd build
qmake ..
make install
```

## 制作软件包
运行

`./dpkg-buildpackage.sh`

