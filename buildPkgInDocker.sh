# 确保 已经配置好docker

docker rm ccc
docker rmi -f ccc-app-manager

docker build -t ccc-app-manager .
docker run -i --name=ccc -w /a ccc-app-manager
docker cp ccc:/target ../

docker rm ccc
docker rmi -f ccc-app-manager
