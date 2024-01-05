import os
import sys
import time
import subprocess
from datetime import datetime


index = 1

def increase_file_sizes(path, index):
    for item in os.listdir(path):
        item_path = os.path.join(path, item)
        if os.path.isfile(item_path):
            # 如果是文件，增加文件大小的10%
            original_size = os.path.getsize(item_path)
            new_size = int(original_size * 1.1)
            print("[handel] {}: {:5.4}MB -> {:5.4}MB".format(path, original_size/(1024**2), new_size/(1024**2)))
            with open(item_path, 'a') as f:
                f.write("\n")
                while original_size < new_size:
                    f.write(f"这是测试内容 this is test content *{index}* \n")
                    original_size = os.path.getsize(item_path)
                
        elif os.path.isdir(item_path):
            # 如果是文件夹，递归调用自身
            increase_file_sizes(item_path, index)

dir_path = "/root/func_test"
if not os.path.exists(dir_path):
    os.makedirs(dir_path)
for i in range(1, 3):
    print(f"正在生成文件: test{i}...")
    file_path = os.path.join(dir_path, f"test{i}")
    file = open(file_path, "w")
    count = 1
    for j in range(1, 5):
        file.write(f"第 {count} 行: 空行 \n")
        count += 1
        for k in range(1, 50):
            file.write(f"第 {count} 行: 第{j}次更新 这是文件{i}的测试内容 \n")
            count += 1
        file.flush()

    file.close()
    print(f"生成文件test{i}完成!!!\n")

dir_path = "/root/func_test/subdir"
if not os.path.exists(dir_path):
    os.makedirs(dir_path)
for i in range(3, 5):
    print(f"正在生成文件: test{i}...")

    file_path = os.path.join(dir_path, f"test{i}")
    file = open(file_path, "w")
    count = 1
    for j in range(1, 5):
        print("-"*25,f"这是文件{i}的第{j}次更新","-"*25)
        file.write(f"第 {count} 行: 空行 \n")
        count += 1
        for k in range(1, 50):
            file.write(f"第 {count} 行: 第{j}次更新 这是文件{i}的测试内容 \n")
            count += 1
        file.flush()
    file.close()
    print(f"生成文件test{i}完成!!!\n")

dir_path = "/root/func_test/subdir/subdir"
if not os.path.exists(dir_path):
    os.makedirs(dir_path)
for i in range(5, 6):
    print(f"正在生成文件: test{i}...")

    file_path = os.path.join(dir_path, f"test{i}")
    file = open(file_path, "w")
    count = 1
    for j in range(1, 5):
        print("-"*25,f"这是文件{i}的第{j}次更新","-"*25)
        file.write(f"第 {count} 行: 空行 \n")
        count += 1
        for k in range(1, 50):
            file.write(f"第 {count} 行: 第{j}次更新 这是文件{i}的测试内容 \n")
            count += 1
        file.flush()
    file.close()
    print(f"生成文件test{i}完成!!!\n")

for i in range(0,7):
    increase_file_sizes('/root/func_test', index)
    # rsync命令参数
    src_path = f'/root/func_test/'
    dest_path = f'rsync_backup@172.26.241.80::backup/func_test2/'
    password_file = '/etc/rsync.password'
    port = '874'
    backup_version = f'2023-08-18-{10+i}:25:00'

    # 记录开始时间
    start_time = time.time()

    # 发起rsync系统调用
    cmd = f"rsync -av {src_path} {dest_path} --password-file={password_file} --port={port} --backup_type=0 --backup_version_num=3 --backup_version={backup_version}"

    print("!cmd: {}".format(cmd))

    subprocess.run(cmd, shell=True)

    index += 1
    time.sleep(1)

for i in range(0,7):
    increase_file_sizes('/root/func_test', index)
    # rsync命令参数
    src_path = f'/root/func_test/'
    dest_path = f'rsync_backup@172.26.241.80::backup/func_test2/'
    password_file = '/etc/rsync.password'
    port = '874'
    backup_version = f'2023-08-17-{10+i}:30:00'

    # 记录开始时间
    start_time = time.time()

    # 发起rsync系统调用
    cmd = f"rsync -av {src_path} {dest_path} --password-file={password_file} --port={port} --backup_type=1 --backup_version_num=3 --backup_version={backup_version}"

    print("!cmd: {}".format(cmd))

    subprocess.run(cmd, shell=True)

    index += 1
    time.sleep(1)