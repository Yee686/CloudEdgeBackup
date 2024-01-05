import os
import sys
import time
import subprocess

src_fold = "/sync_worker/my_test2/file_size_test_2/"
dst_fold = "rsync_backup@172.17.0.3::backup/"

password = "/rsync.passwd"

log_fold = "/sync_worker/my_test2/incre_backup_log/"

backup_version_num = 3

backup_version = "2023-23-27-01:00:00"

for size in ['8KB', '128KB', '2MB']:
# for size in ['2MB']:

    src_path = src_fold + size + "/"
    dst_path = dst_fold + size + "/"

    if not os.path.exists(log_fold):
        os.mkdir(log_fold)

    log_path = log_fold + size + ".txt"
    

    # cmd = f"trickle -u 50 -d 50 rsync -av {src_path} {dst_path} --password-file={password} --port=873 --backup_type={0} --backup_version_num={backup_version_num} --backup_version={backup_version} > {log_path}"
    cmd = f"trickle -s -u 500 -d 500 rsync -av {src_path} {dst_path} --password-file={password} --port=873 --backup_type={0} --backup_version_num={backup_version_num} --backup_version={backup_version}"

    print(cmd)

    process = subprocess.Popen(cmd, shell=True)
    process.wait()  # 等待命令执行完成

    print("full backup {} done\n".format(size))

    # process = subprocess.Popen("rm -rf /home/7948lkj/CloudEdgeBackup/test/file_size_test/KB+/1024K/*", shell=True)
    # process.wait()  # 等待命令执行完成

    # process = subprocess.Popen("cp -r test/file_size_test/KB/1024K/* /home/7948lkj/CloudEdgeBackup/test/file_size_test/KB+/1024K/", shell=True)
    # process.wait()  # 等待命令执行完成
