FROM livegrep/base
RUN apt-get update && apt-get -y dist-upgrade
RUN DEBIAN_FRONTEND=noninteractive apt-get -y install nginx

# the base image adds ./livegrep/nginx.conf for us, messy, but
# works for now

CMD nginx -c /livegrep/nginx.conf
