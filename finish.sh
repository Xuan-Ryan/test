# un-tart this file
tar xvjf MT7620-MT7612.tar.bz2

# revert all files from SVN
svn revert -R RT288x_SDK/source/*
svn revert -R RT288x_SDK/Uboot/*
svn revert    RT288x_SDK/source/Makefile
svn revert -R RT288x_SDK/source/final/*
svn revert -R RT288x_SDK/source/mctconfig/*
svn revert -R RT288x_SDK/source/vendors/*
svn revert    RT288x_SDK/source/user/Makefile
svn revert -R RT288x_SDK/source/user/lighttpd-1.4.20/*
svn revert -R RT288x_SDK/source/user/rt2880_app/nvram/*
svn revert -R RT288x_SDK/source/user/rt2880_app/scripts/*
svn revert -R RT288x_SDK/source/user/alsa-lib-1.0.27.2/*
# if we add more files in source tree we need to add here too.

# remove the archive file, which we don't need it.
rm MT7620-MT7612.tar.bz2
