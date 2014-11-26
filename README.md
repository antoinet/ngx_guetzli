# What is this?
This project is my first attempt of writing a nginx module. It's purpose is
to implement a cookie store for proxied web servers, that is: all cookies
set by backend servers are kept by the module. In turn, the module sets
a single session cookie, by which an individual store is identified.

# How to build
Install required packages:
    $ sudo apt-get install build-essential libpcre3-dev zlib1g-dev \
      libcurl4-openssl-dev redis-server libhiredis-dev libhiredis0.10 \
      uuid-dev

# References
* http://www.evanmiller.org/nginx-modules-guide.html
* https://www.airpair.com/nginx/extending-nginx-tutorial
* http://de.slideshare.net/trygvevea/extending-functionality-in-nginx-with-modules
* http://wiki.nginx.org/HeadersManagement
* http://www.nginxguts.com/category/nginx/
* http://www.nginxguts.com/2011/01/working-with-cookies/
* https://github.com/eliast/nginx-uuid-filter
* http://lxr.nginx.org/source/src/http/modules/ngx_http_userid_filter_module.c

