iOS Push Email for Dovecot
==========================

What is this?
-------------

This project, together with the [dovecot-xaps-daemon](https://github.com/freswa/dovecot-xaps-daemon) project, will enable push email for iOS devices that talk to your Dovecot 2.4.x IMAP server. This is specially useful for people who are migrating away from running email services on OS X Server and want to keep the Push Email ability.

> Please note that it is not possible to use this project without legally owning a copy of OS X Server. You can purchase OS X Server on the [Mac App Store](https://itunes.apple.com/ca/app/os-x-server/id714547929?mt=12) or download it for free if you are a registered Mac or iOS developer.

High Level Overview
-------------------

There are two parts to enabling iOS Push Email. You will need both parts for this to work.

First you need to install the Dovecot plugins from this project. The Dovecot plugins add support for the `XAPPLEPUSHSERVICE` IMAP extension that will let iOS devices register themselves to receive native push notifications for new email arrival.

(Apple did not document this feature, but it did publish the source code for all their Dovecot patches on the [Apple Open Source project site](http://www.opensource.apple.com/source/dovecot/dovecot-293/), which include this feature. So although I was not able to follow a specification, I was able to read their open source project and do a clean implementation with all original code.)

Second, you need to install a daemon process, from the [dovecot-xaps-plugin](https://github.com/freswa/dovecot-xaps-daemon) project, that will be responsible for receiving new email notifications from the Dovecot Local Delivery Agent or from the Dovecot LMTP server and transforming those into native Apple Push Notifications.

Installation
============

Prerequisites
-------------

You are going to need the following things to get this going:

* Some patience and willingness to experiment - Although I run this project in production, it is still a very early version and it may contain bugs.
* Dovecot > 2.3.0 (which introduced some needed features to the http_client implementation of dovecot) 

> Note that you need to have an existing Dovecot setup working. Either with local system users or with virtual users. Also note that you need to be using the Dovecot Local Delivery Agent or the Dovecot LMTP server for this to work. The [Dovecot LDA](http://wiki2.dovecot.org/LDA) and the [LMTP server](http://wiki2.dovecot.org/LMTP) are described in detail on the Dovecot Wiki

Installing the Dovecot plugins
------------------------------

### Debian package

Tagged releases include a Debian package built against Dovecot 2.4.2. Download
the `.deb` file for the release and install it with APT so its Dovecot dependency
is checked automatically:

```
sudo apt install ./dovecot-xaps-plugin_<version>_<architecture>.deb
```

The package installs the plugin modules in `/usr/lib/dovecot/modules/`, the
Dovecot settings module in `/usr/lib/dovecot/modules/settings/`, and the
configuration as `/etc/dovecot/conf.d/95-xaps.conf`.

### Build from source

Ubuntu 26.04 LTS is the first Ubuntu LTS release that includes Dovecot 2.4.2.
Install its build tools and Dovecot development package:

```
sudo apt-get update
sudo apt-get install build-essential git dovecot-dev cmake
```

Clone this project:

```
git clone https://github.com/freswa/dovecot-xaps-plugin.git
cd dovecot-xaps-plugin
```

Compile and install the plugins. Note that the installation destination in the `Makefile` is hardcoded for Ubuntu, it expects the Dovecot modules to live at `/usr/lib/dovecot/modules/`. You can either modify the `Makefile` or copy the modules to the right place manually.

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
sudo make install
```

Install the configuration file. Also specific for Ubuntu, may be different for your operating system.

```
sudo cp xaps.conf /etc/dovecot/conf.d/95-xaps.conf
```

In the configuration file, change the `xaps_socket` option to point to the same location as you specified on the `xapsd` daemon arguments.

Restart Dovecot:

```
sudo service dovecot restart
```

Building a Debian package
-------------------------

After installing the build prerequisites above, CPack can create the same
package produced by the release pipeline:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DXAPS_VERSION=1.0.0
cmake --build build --parallel
cpack --config build/CPackConfig.cmake -G DEB -B dist
```

CI builds every pull request and push on Ubuntu 26.04 LTS against its Dovecot
2.4.2 packages. Pushing a stable version tag such as `v1.1.0` builds the Debian
package and attaches it to the corresponding GitHub release.

Debugging
---------

Put a tail on `/var/log/mail.log` and keep an eye on the output of the `xapsd` daemon. (See instructions in that project). If you see any errors or core dumps, please [file a bug](https://github.com/freswa/dovecot-xaps-plugin/issues/new).
