#!/bin/bash

# 获取版本号
if [[ "" == $version ]] || [[ "\n" == $version ]]
then
    read -p "version: " version
fi

rm -rf dpkg-build-tmp
mkdir -p dpkg-build-tmp
cd dpkg-build-tmp
cp -rf ../* .

cat << EOF > debian/changelog
com.github.ccc-app-manager ($version) unstable; urgency=medium

  * Initial release (Closes: #nnnn)  <nnnn is the bug number of your ITP>

 -- keke <ct243768648@qq.com>  Tue, 9 Sep 2022 10:42:29 +0800
EOF

#更新info
sed -i "s/\ \"version\":.*/\ \"version\": $version,/g" debian/deepin_pkg_info/info

dpkg-buildpackage -us -uc

cd ..
rm -rf dpkg-build-tmp