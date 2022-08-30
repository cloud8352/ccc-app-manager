echo "deb https://mirrors.huaweicloud.com/debian/ buster main contrib non-free" > /etc/apt/sources.list
# 换源
apt update
export DEBIAN_FRONTEND=noninteractive
echo "安装依赖..."
apt-get install libgsettings-qt-dev -y 
apt-get install git curl fakeroot qtbase5-dev zlib1g-dev qt5-default  -y 
git clone https://gitlink.org.cn/shenmo7192/dtk-old-bundle.git
cd dtk-old-bundle
apt install ./*.deb -y
cd ..
rm -rf dtk-old-bundle

cd gitee-go-build

cd build-package
./build.sh

mkdir target

for f in $(find . -type f -name "*.deb")
do
    mv $f target
done

mv target /
