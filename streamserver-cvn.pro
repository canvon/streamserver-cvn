TEMPLATE = subdirs

SUBDIRS += \
    libinfra \
    libmedia \
    streamserver-cvn-cli \
    ts-dump \
    ts-split \
    libmedia-test

libmedia.depends             = libinfra
streamserver-cvn-cli.depends = libinfra libmedia
ts-dump.depends              = libinfra libmedia
ts-split.depends             = libinfra libmedia
libmedia-test.depends        = libmedia
