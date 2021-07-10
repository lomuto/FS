# File System

Implementation of File System at application level

</br>

## Introduction

Emulation of a simple file system

- Block size: 4KB
- inode size: 256B

</br>

## Overall of Disk

![disk](./images/overall.PNG)

- S: Super Block
- ibmap: Inode bitmap
- dbmap: Data region bitmap
- iblock: Inodes
- D: Data region

</br>

## Data structures

### block

![block](./images/block.jpg)

### Inode

![inode](./images/inode.PNG)

### Root Directory

![root_dir](./images/root_dir.jpg)

### Inode freelist

![inode_freelist](./images/inode_freelist.PNG)

### Root Directory freelist

![root_dir_freelist](./images/root_dir_freelist.PNG)

</br>

## Run

`$ fs <input_file>`
