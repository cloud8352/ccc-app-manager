sed -i 's/deb.debian.org/mirrors.ustc.edu.cn/g' /etc/apt/sources.list
# 换源
apt update
export DEBIAN_FRONTEND=noninteractive
echo "安装依赖..."
apt install git curl fakeroot qtbase5-dev zlib1g-dev -y 
git clone https://gitlink.org.cn/shenmo7192/dtk-old-bundle.git
cd dtk-old-bundle
apt install ./*.deb -y
cd ..
rm -rf dtk-old-bundle

cd gitee-go-build

build-package/build.sh

cd build-package
mkdir target

for f in $(find . -type f -name "*.deb")
do
    mv $f target
done

mv target /
