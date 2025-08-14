![NumbFS-utils](https://img.shields.io/badge/Project-NumbFS-green)
[![License: GPL-2.0-only](https://img.shields.io/badge/License-GPL%202.0--only-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

# NumbFS-utils
User-space utilities for the NumbFS filesystem.

## Overview
NumbFS-utils provides essential tools to create, manage, and inspect disk images for the **NumbFS** filesystem. It includes:

- `mkfs.numbfs`: Formats a block device or file as a NumbFS partition.
- `fsck.numbfs`: Print file system information.

## Prerequisites
Build tools:
```bash
# Debian/Ubuntu
sudo apt install meson (>=0.50) ninja-build
# Fedora
sudo dnf install meson ninja-build
```
## Installation
1. Clone the repository:
```bash
https://github.com/salvete/NumbFS-utils.git
cd NumbFS-utils
```

2. Build and install:
```bash
meson setup build  # or meson setup build -Dnumbfs_debug=true
ninja -C build
sudo ninja -C build install  # Installs to /usr/local/bin by default
```

## Usage
### 1. Create a filesystem image
```bash
mkfs.numbfs /path/to/device_or_image_file
```
Example:
```bash
mkfs.numbfs disk.img  # Creates a NumbFS image on regular file

or

mfks.numbfs /dev/vdc # Creates a NumbFS image on bloce device
```

### 2. Check an image
```bash
fsck.numbfs /path/to/image
```
Example:
```bash
# testfile is a 10MB NumbFS filesystem image with root inode ID 1
$ fsck.numbfs --inodes --blocks --nid 1 ./testfile

Superblock Information
    inode bitmap start:         2
    inode zone start:           3
    block bitmap start:         515
    data zone start:            520
    total inodes:               4096
    total free data blocks:     19959
    inodes usage:               0.00%
    blocks usage:               0.01%
================================
Inode Information
    inode number:               1
    inode type:                 DIR
    link count:                 2
    inode size:                 128

    DIR CONTENT
       INODE: 00001, NAME: ..
       INODE: 00001, NAME: .
```

## Options
View tool-specific flags:
```bash
mkfs.numbfs --help
fsck.numbfs --help
```
## Contributing
Patches are welcome! Submit issues or PRs via GitHub.

## License
This project is licensed under the GNU General Public License v2.0 only (GPL-2.0-only).

- See [LICENSE](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html) for full terms.

- **Modifications** must be disclosed under the same license.
