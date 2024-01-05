import os
import os
import sys
import time
import subprocess

src_path = "/sync_worker/matric/backup_test_data/"
dst_path = "rsync_backup@172.17.0.3::backup/"

password = "/rsync.passwd"

log_fold = "/sync_worker/metric/backup_log/"

backup_version_num = 5

backup_version_delta = "2024-01-05-01:00:00"

if __name__ == '__main__':
    
    '''fibre'''
    cmd = f"rsync --stats -av {src_path}fibre {dst_path} --password-file={password} --port=873 --backup_type={0} \
    --backup_version_num={backup_version_num} --backup_version={backup_version_delta}"

    print(cmd)

    process = subprocess.Popen(cmd, shell=True)
    process.wait()  # 等待命令执行完成

    print("fibre backup done\n")

    '''micro'''
    cmd = f"rsync --stats -a {src_path}micro {dst_path} --password-file={password} --port=873 --backup_type={0} \
    --backup_version_num={backup_version_num} --backup_version={backup_version_delta}"

    print(cmd)

    process = subprocess.Popen(cmd, shell=True)
    process.wait()  # 等待命令执行完成

    print("fibre backup done\n")

    '''ray'''
    cmd = f"rsync --stats -av {src_path}ray {dst_path} --password-file={password} --port=873 --backup_type={0} \
    --backup_version_num={backup_version_num} --backup_version={backup_version_delta}"

    print(cmd)

    process = subprocess.Popen(cmd, shell=True)
    process.wait()  # 等待命令执行完成

    print("ray backup done\n")

