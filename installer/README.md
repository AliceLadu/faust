
# Installer tools # 

In this folder you will find various installation scripts that take care of installing the dependencies needed to work with Faust on a Linux machine. These scripts are useful if you want to compile Faust yourself and use the latest git version.

The scripts are organized in plateforms (currently only ubuntu1404) and four levels of installation:

- `unbuntu1404`
- `install.min.sh`        : level1, install the minimal dependencies to compiler faust, master branch
- `install.regular.sh`    : level2, add additional packages to create alsa and JACK applications
- `install.developper.sh` : level3, add packages to compile audio applications and plugins for most formats 
- `install.full.sh`       : level4, install everything including latex for automatique documentation

