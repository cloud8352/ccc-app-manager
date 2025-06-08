# 换源
pwd
echo "换源"
cat > /etc/apt/sources.list << EOF
# 默认注释了源码镜像以提高 apt update 速度，如有需要可自行取消注释
deb http://mirrors.tuna.tsinghua.edu.cn/debian/ buster main contrib non-free
# deb-src http://mirrors.tuna.tsinghua.edu.cn/debian/ buster main contrib non-free
deb http://mirrors.tuna.tsinghua.edu.cn/debian/ buster-updates main contrib non-free
# deb-src http://mirrors.tuna.tsinghua.edu.cn/debian/ buster-updates main contrib non-free
deb http://mirrors.tuna.tsinghua.edu.cn/debian-security buster/updates main contrib non-free
# deb-src http://mirrors.tuna.tsinghua.edu.cn/debian-security buster/updates main contrib non-free
EOF
cat /etc/apt/sources.list

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
