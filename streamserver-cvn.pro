TEMPLATE = subdirs

SUBDIRS += \
    libinfra \
    libmedia \
    streamserver-cvn-cli \
    ts-dump \
    ts-split \
    tests

libmedia.depends             = libinfra
streamserver-cvn-cli.depends = libinfra libmedia
ts-dump.depends              = libinfra libmedia
ts-split.depends             = libinfra libmedia
tests.depends                = libinfra libmedia streamserver-cvn-cli ts-dump ts-split
