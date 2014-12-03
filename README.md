# What is this?
This project is my first attempt of writing a nginx module. It's purpose is
to implement a cookie store for proxied web servers, that is: all cookies
set by backend servers are kept by the module. In turn, the module sets
a single session cookie, by which an individual store is identified.

# How to build
Install required packages:
```
$ sudo apt-get install build-essential libpcre3-dev zlib1g-dev \
  libcurl4-openssl-dev redis-server libhiredis-dev libhiredis0.10 \
  uuid-dev
```

Install ruby, e.g.
```
$ curl -sSL https://get.rvm.io | bash -s stable
```

Bootstrap the development environment
```
$ rake bootstrap
```

Build nginx together with the module
```
$ rake nginx:compile
```

Run a sample backend server
```
$ ruby backend_app.rb
```

Run the integration tests
```
$ rake nginx:integration
```

# References
## Tutorials and Blogs
* [Emiller's Guide To Nginx Module Development](http://www.evanmiller.org/nginx-modules-guide.html)
* [NGINX Tutorial: Developing Modules](https://www.airpair.com/nginx/extending-nginx-tutorial)
* [Extending functionality in nginx, with modules!](http://de.slideshare.net/trygvevea/extending-functionality-in-nginx-with-modules)
* [nginx wiki: Managing request headers](http://wiki.nginx.org/HeadersManagement)
* [Nginx Guts, a blog about website scalability, web technologies and more.](http://www.nginxguts.com/category/nginx/)
* [Nginx Guts: Working with cookies in a Nginx module](http://www.nginxguts.com/2011/01/working-with-cookies/)
* [Development of modules for nginx](http://antoine.bonavita.free.fr/nginx_mod_dev_en.html)

## Reference Modules
* [nginx-uuid-filter:  An nginx module for generating cookies using libuuid and hex format ](https://github.com/eliast/nginx-uuid-filter)
* [ngx-http-userid-filter-module](http://lxr.nginx.org/source/src/http/modules/ngx_http_userid_filter_module.c)
* [nginx-http-headers-more-module: Set, add, and clear arbitrary output headers](https://github.com/openresty/headers-more-nginx-module)

## Mailing lists
* [Removing a request header in an access phase handler](http://forum.nginx.org/read.php?2,240671,248180#msg-248180)
