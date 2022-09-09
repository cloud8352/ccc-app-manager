#!/bin/bash
read -p "version: " version

mkdir -p dpkg-build-tmp
cd dpkg-build-tmp
cp -rf ../* .

cat << EOF > debian/changelog
com.github.ccc-app-manager ($version) unstable; urgency=medium

  * Initial release (Closes: #nnnn)  <nnnn is the bug number of your ITP>

 -- keke <ct243768648@qq.com>  Tue, 9 Sep 2022 10:42:29 +0800
EOF

dpkg-buildpackage -us -uc

cd ..
rm -rf dpkg-build-tmp