import os
import subprocess
import time
import csv

# FULL = True
# DELTA = True
# RECOVERY = False

FULL = False
DELTA = False
RECOVERY = True

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


if FULL :
    print("开始全量同步")
    with open('/home/7948lkj/CloudEdgeBackup/CloudEdgeBackup/my_test/file_size_test_archive/file_size_sync_test_full.csv', 'w') as f:
        writer = csv.writer(f)
        writer.writerow(['运行时间', '文件大小'])

        test_file_size = 1
        for i in range(1,16):

            # rsync命令参数
            src_path = f'/home/7948lkj/CloudEdgeBackup/CloudEdgeBackup/my_test/file_size_test/{test_file_size}k/'
            dest_path = f'rsync_backup@192.168.0.9::backup/file_size_test/{test_file_size}k/'
            password_file = '/home/7948lkj/CloudEdgeBackup/CloudEdgeBackup/rsync.password'
            port = '873'
            backup_version = '2023-07-25-22:25:00'

            # 记录开始时间
            start_time = time.time()

            # 发起rsync系统调用
            cmd = f"rsync -av {src_path} {dest_path} --password-file={password_file} --port={port} --backup_version={backup_version}"

            print("!cmd: {}".format(cmd))

            subprocess.run(cmd, shell=True)

            # 计算运行时间和文件大小
            end_time = time.time()
            elapsed_time = end_time - start_time
            output = subprocess.check_output(f"du -sh {src_path}", shell=True)
            file_size = output.decode().split()[0]

            # 将结果写入csv文件
            writer.writerow([elapsed_time, file_size])

            test_file_size = test_file_size * 2

if DELTA:
    print("开始增量同步")
    for delta_time in range(1,4):
        with open(f'/home/7948lkj/CloudEdgeBackup/CloudEdgeBackup/my_test/file_size_test_archive/file_size_sync_test_delta_{delta_time}.csv', 'w') as f:

            writer = csv.writer(f)
            writer.writerow(['运行时间', '文件大小'])
            test_file_size = 1

            increase_file_sizes("/home/7948lkj/CloudEdgeBackup/CloudEdgeBackup/my_test/file_size_test/", delta_time)

            for i in range(1,16):

                # rsync命令参数
                src_path = f'/home/7948lkj/CloudEdgeBackup/CloudEdgeBackup/my_test/file_size_test/{test_file_size}k/'
                dest_path = f'rsync_backup@192.168.0.9::backup/file_size_test/{test_file_size}k/'
                password_file = '/home/7948lkj/CloudEdgeBackup/CloudEdgeBackup/rsync.password'
                port = '873'
                backup_version = f'2023-07-25-23:{delta_time}0:00'

                # 记录开始时间
                start_time = time.time()

                # 发起rsync系统调用
                cmd = f"rsync -av {src_path} {dest_path} --password-file={password_file} --port={port} --backup_version={backup_version}"

                print("!cmd: {}".format(cmd))

                subprocess.run(cmd, shell=True)

                # 计算运行时间和文件大小
                end_time = time.time()
                elapsed_time = end_time - start_time
                output = subprocess.check_output(f"du -sh {src_path}", shell=True)
                file_size = output.decode().split()[0]

                # 将结果写入csv文件
                writer.writerow([elapsed_time, file_size])

                test_file_size = test_file_size * 2

if RECOVERY :
    print("开始恢复")
    for delta_time in range(1,4):
        with open(f'/home/7948lkj/CloudEdgeBackup/CloudEdgeBackup/my_test/file_size_test_archive/file_size_sync_test_delta_recovery.csv', 'w') as f:

            writer = csv.writer(f)
            writer.writerow(['运行时间', '文件大小'])
            test_file_size = 1

            for i in range(1,16):

                # rsync命令参数
                src_path = f'rsync_backup@192.168.0.9::backup/file_size_test/{test_file_size}k/'
                dest_path = f'/home/7948lkj/CloudEdgeBackup/CloudEdgeBackup/my_test/file_size_test/{test_file_size}k/'
                password_file = '/home/7948lkj/CloudEdgeBackup/CloudEdgeBackup/rsync.password'
                port = '873'
                recovery_version = f'2023-07-26-00:{delta_time}0:00'

                # 记录开始时间
                start_time = time.time()

                # 发起rsync系统调用
                cmd = f'''rsync -av --exclude="*.backup/" {src_path} {dest_path} --password-file={password_file} --port={port} --recovery_version={recovery_version}'''

                print("!cmd: {}".format(cmd))

                subprocess.run(cmd, shell=True)

                # 计算运行时间和文件大小
                end_time = time.time()
                elapsed_time = end_time - start_time
                output = subprocess.check_output(f"du -sh {dest_path}", shell=True)
                file_size = output.decode().split()[0]

                # 将结果写入csv文件
                writer.writerow([elapsed_time, file_size])

                test_file_size = test_file_size * 2

