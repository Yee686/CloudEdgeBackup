import os
import os
import sys
import time
import subprocess

src_fold = "/sync_worker/matric/backup_test_data/ray"
dst_fold = "rsync_backup@172.17.0.3::backup/ray"

password = "/rsync.passwd"

log_fold = "/sync_worker/matric/backup_log/"

backup_version_num = 5

backup_version_delta = "2024-01-05-02:00:00"

if __name__ == '__main__':
    src_path = src_fold + "/"
    dst_path = dst_fold 

    if not os.path.exists(log_fold):
        os.mkdir(log_fold)

    log_path = log_fold + "ray.txt"
    

    cmd = f"trickle -s -u 7 -d 7 rsync --stats -av {src_path} {dst_path} --password-file={password} --port=873 \
    --backup_type={0} --backup_version_num={backup_version_num} --backup_version={backup_version_delta} > {log_path}"

    print(cmd)

    process = subprocess.Popen(cmd, shell=True)
    process.wait()  # 等待命令执行完成

    print("ray backup done\n")