import subprocess
import os
import time
from datetime import datetime

dir_path = "/root/func_test"
if not os.path.exists(dir_path):
    os.makedirs(dir_path)
for i in range(1, 3):
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

        now = datetime.now()
        current_time = now.strftime("%Y-%m-%d-%H:%M:%S")
        backup_version = "--backup_version=" + current_time

        subprocess.run(["rsync", "-av", "/root/func_test/", "rsync_backup@172.25.78.135::backup/func_test/", "--password-file=/etc/rsync.password", "--port=874",backup_version])
        time.sleep(3)

        print("-"*25,f"文件{i}的第{j}次更新完成!!","-"*25)
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

        now = datetime.now()
        current_time = now.strftime("%Y-%m-%d-%H:%M:%S")
        backup_version = "--backup_version=" + current_time

        subprocess.run(["rsync", "-av", "/root/func_test/", "rsync_backup@172.25.78.135::backup/func_test/", "--password-file=/etc/rsync.password", "--port=874",backup_version])
        time.sleep(3)

        print("-"*25,f"文件{i}的第{j}次更新完成!!","-"*25)
    file.close()
    print(f"生成文件test{i}完成!!!\n")