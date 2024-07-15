FROM docker.io/debian:10

RUN mkdir -p /a

COPY . /a

CMD ["/bin/cd", "/a"]
CMD ["/bin/bash", "/a/build-on-gitee.sh"]
