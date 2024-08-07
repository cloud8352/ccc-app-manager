# 换源
pwd
echo "换源"
sed -i 's/deb.debian.org/mirrors.ustc.edu.cn/g' /etc/apt/sources.list
sed -i 's/deb http:\/\/security.debian.org/#deb http:\/\/security.debian.org/g' /etc/apt/sources.list

apt-get update
export DEBIAN_FRONTEND=noninteractive
echo "安装依赖..."
apt-get install libgsettings-qt-dev -y 
apt-get install debhelper git curl fakeroot qtbase5-dev zlib1g-dev qt5-default  -y 
git clone https://gitlink.org.cn/shenmo7192/dtk-old-bundle.git
cd dtk-old-bundle
apt-get install ./*.deb -y
cd ..
rm -rf dtk-old-bundle

export version=${WORK_FLOW_ENV_PKG_VERSION}
./dpkg-buildpackage.sh

mkdir -p ../target

output_file_path_list=$(find . -maxdepth 1 -type f -name "com.github.ccc-app-manager_*" -o -name "com.github.ccc-app-manager-dbgsym_*")
for f in ${output_file_path_list}
do
    mv $f ../target
done

echo "target:"
ls ../target
